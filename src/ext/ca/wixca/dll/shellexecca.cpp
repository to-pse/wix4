//-------------------------------------------------------------------------------------------------
// <copyright file="shellexecca.cpp" company="Outercurve Foundation">
//   Copyright (c) 2004, Outercurve Foundation.
//   This software is released under Microsoft Reciprocal License (MS-RL).
//   The license and further copyright text can be found in the file
//   LICENSE.TXT at the root directory of the distribution.
// </copyright>
// 
// <summary>
//    Executes default shell verb on a specified object (file, URL, and so forth).
// </summary>
//-------------------------------------------------------------------------------------------------

#include "precomp.h"

HRESULT ShellExec(
    __in LPCWSTR wzTarget,
    __in BOOL fUnelevated
    )
{
    HRESULT hr = S_OK;
    LPWSTR sczWorkingDirectory = NULL;

    // a reasonable working directory (not the system32 default from MSI) is the directory where the target lives
    hr = PathGetDirectory(wzTarget, &sczWorkingDirectory);
    ExitOnFailure(hr, "failed to get directory for target: %ls", wzTarget);
    
    if (!DirExists(sczWorkingDirectory, NULL))
    {
        ReleaseNullStr(sczWorkingDirectory);
    }

    if (fUnelevated)
    {
        hr = ShelExecUnelevated(wzTarget, NULL, NULL, sczWorkingDirectory, SW_SHOWDEFAULT);
        ExitOnFailure(hr, "ShelExecUnelevated failed with target %ls", wzTarget);
    }
    else
    {
        HINSTANCE hinst = ::ShellExecuteW(NULL, NULL, wzTarget, NULL, sczWorkingDirectory, SW_SHOWDEFAULT);
        if (hinst <= HINSTANCE(32))
        {
            switch (int(hinst))
            {
            case ERROR_FILE_NOT_FOUND:
                hr = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
                break;
            case ERROR_PATH_NOT_FOUND:
                hr = HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
                break;
            case ERROR_BAD_FORMAT:
                hr = HRESULT_FROM_WIN32(ERROR_BAD_FORMAT);
                break;
            case SE_ERR_ASSOCINCOMPLETE:
            case SE_ERR_NOASSOC:
                hr = HRESULT_FROM_WIN32(ERROR_NO_ASSOCIATION);
                break;
            case SE_ERR_DDEBUSY:
            case SE_ERR_DDEFAIL:
            case SE_ERR_DDETIMEOUT:
                hr = HRESULT_FROM_WIN32(ERROR_DDE_FAIL);
                break;
            case SE_ERR_DLLNOTFOUND:
                hr = HRESULT_FROM_WIN32(ERROR_DLL_NOT_FOUND);
                break;
            case SE_ERR_OOM:
                hr = E_OUTOFMEMORY;
                break;
            case SE_ERR_ACCESSDENIED:
                hr = E_ACCESSDENIED;
                break;
            default:
                hr = E_FAIL;
            }

            ExitOnFailure(hr, "ShellExec failed with return code %d", int(hinst));
        }
    }


LExit:
    ReleaseStr(sczWorkingDirectory);
    return hr;
}

extern "C" UINT __stdcall WixShellExec(
    __in MSIHANDLE hInstall
    )
{
    Assert(hInstall);
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;
    LPWSTR pwzTarget = NULL;

    hr = WcaInitialize(hInstall, "WixShellExec");
    ExitOnFailure(hr, "failed to initialize");

    hr = WcaGetFormattedProperty(L"WixShellExecTarget", &pwzTarget);
    ExitOnFailure(hr, "failed to get WixShellExecTarget");

    WcaLog(LOGMSG_VERBOSE, "WixShellExecTarget is %ls", pwzTarget);

    if (!pwzTarget || !*pwzTarget)
    {
        hr = E_INVALIDARG;
        ExitOnFailure(hr, "failed to get WixShellExecTarget");
    }

    hr = ShellExec(pwzTarget, FALSE);
    ExitOnFailure(hr, "failed to launch target");

LExit:
    ReleaseStr(pwzTarget);

    if (FAILED(hr)) 
    {
        er = ERROR_INSTALL_FAILURE;
    }
    return WcaFinalize(er); 
}

extern "C" UINT __stdcall WixUnelevatedShellExec(
    __in MSIHANDLE hInstall
    )
{
    Assert(hInstall);
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;
    LPWSTR pwzTarget = NULL;

    hr = WcaInitialize(hInstall, "WixUnelevatedShellExec");
    ExitOnFailure(hr, "failed to initialize");

    hr = WcaGetFormattedProperty(L"WixUnelevatedShellExecTarget", &pwzTarget);
    ExitOnFailure(hr, "failed to get WixUnelevatedShellExecTarget");

    WcaLog(LOGMSG_VERBOSE, "WixUnelevatedShellExecTarget is %ls", pwzTarget);

    if (!pwzTarget || !*pwzTarget)
    {
        hr = E_INVALIDARG;
        ExitOnFailure(hr, "failed to get WixShellExecTarget");
    }

    hr = ShellExec(pwzTarget, TRUE);
    ExitOnFailure(hr, "failed to launch target");

LExit:
    ReleaseStr(pwzTarget);

    if (FAILED(hr)) 
    {
        er = ERROR_INSTALL_FAILURE;
    }
    return WcaFinalize(er); 
}

//
// ExtractBinary extracts the data from the Binary table row with the given ID into a file. 
//
HRESULT ExtractBinary(
    __in LPCWSTR wzBinaryId,
    __out BYTE** pbData,
    __out DWORD* pcbData
    )
{
    HRESULT hr = S_OK;
    LPWSTR pwzSql = NULL;
    PMSIHANDLE hView;
    PMSIHANDLE hRec;

    // make sure we're not horked from the get-go
    hr = WcaTableExists(L"Binary");
    if (S_OK != hr)
    {
        if (SUCCEEDED(hr))
        {
            hr = E_UNEXPECTED;
        }
        ExitOnFailure(hr, "There is no Binary table.");
    }

    ExitOnNull(wzBinaryId, hr, E_INVALIDARG, "Binary ID cannot be null");
    ExitOnNull(*wzBinaryId, hr, E_INVALIDARG, "Binary ID cannot be empty string");

    hr = StrAllocFormatted(&pwzSql, L"SELECT `Data` FROM `Binary` WHERE `Name`=\'%s\'", wzBinaryId);
    ExitOnFailure(hr, "Failed to allocate Binary table query.");

    hr = WcaOpenExecuteView(pwzSql, &hView);
    ExitOnFailure(hr, "Failed to open view on Binary table");

    hr = WcaFetchSingleRecord(hView, &hRec);
    ExitOnFailure(hr, "Failed to retrieve request from Binary table");

    hr = WcaGetRecordStream(hRec, 1, pbData, pcbData);
    ExitOnFailure(hr, "Failed to read Binary.Data.");

LExit:
    ReleaseStr(pwzSql);

    return hr;
}

extern "C" UINT __stdcall WixShellExecBinary(
    __in MSIHANDLE hInstall
    )
{
    Assert(hInstall);
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;
    LPWSTR pwzBinary = NULL;
    LPWSTR pwzFilename = NULL;
    BYTE* pbData = NULL;
    DWORD cbData = 0;
    HANDLE hFile = INVALID_HANDLE_VALUE;

#if 0
    ::MessageBoxA(0, "WixShellExecBinary", "-->> ATTACH HERE", MB_OK);
#endif

    hr = WcaInitialize(hInstall, "WixShellExecBinary");
    ExitOnFailure(hr, "failed to initialize");

    hr = WcaGetFormattedProperty(L"WixShellExecBinaryId", &pwzBinary);
    ExitOnFailure(hr, "failed to get WixShellExecBinaryId");

    WcaLog(LOGMSG_VERBOSE, "WixShellExecBinaryId is %ls", pwzBinary);

    if (!pwzBinary || !*pwzBinary)
    {
        hr = E_INVALIDARG;
        ExitOnFailure(hr, "failed to get WixShellExecBinaryId");
    }

    // get temporary path for extracted file
    StrAlloc(&pwzFilename, MAX_PATH);
    ExitOnFailure(hr, "Failed to allocate temporary path");
    ::GetTempPathW(MAX_PATH, pwzFilename);
    hr = ::StringCchCatW(pwzFilename, MAX_PATH, pwzBinary);
    ExitOnFailure(hr, "Failed to append filename.");

    // grab the bits
    hr = ExtractBinary(pwzBinary, &pbData, &cbData);
    ExitOnFailure(hr, "failed to extract binary data");

    // write 'em to the temp file
    hFile = ::CreateFileW(pwzFilename, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == hFile)
    {
        ExitWithLastError(hr, "Failed to open new temp file: %ls", pwzFilename);
    }

    DWORD cbWritten = 0;
    if (!::WriteFile(hFile, pbData, cbData, &cbWritten, NULL))
    {
        ExitWithLastError(hr, "Failed to write data to new temp file: %ls", pwzFilename);
    }

    // close it
    ::CloseHandle(hFile);
    hFile = INVALID_HANDLE_VALUE;

    // and run it
    hr = ShellExec(pwzFilename, FALSE);
    ExitOnFailure(hr, "failed to launch target: %ls", pwzFilename);

LExit:
    ReleaseStr(pwzBinary);
    ReleaseStr(pwzFilename);
    ReleaseMem(pbData);
    if (INVALID_HANDLE_VALUE != hFile)
    {
        ::CloseHandle(hFile);
    }

    if (FAILED(hr)) 
    {
        er = ERROR_INSTALL_FAILURE;
    }
    return WcaFinalize(er); 
}
