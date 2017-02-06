// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_CRYPTOHOME_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_CRYPTOHOME_DBUS_CONSTANTS_H_

namespace cryptohome {
const char kCryptohomeInterface[] = "org.chromium.CryptohomeInterface";
const char kCryptohomeServicePath[] = "/org/chromium/Cryptohome";
const char kCryptohomeServiceName[] = "org.chromium.Cryptohome";
// Methods
const char kCryptohomeAsyncAddKey[] = "AsyncAddKey";
const char kCryptohomeCheckKey[] = "CheckKey";
const char kCryptohomeMigrateKey[] = "MigrateKey";
const char kCryptohomeRemove[] = "Remove";
const char kCryptohomeGetSystemSalt[] = "GetSystemSalt";
const char kCryptohomeGetSanitizedUsername[] = "GetSanitizedUsername";
const char kCryptohomeIsMounted[] = "IsMounted";
const char kCryptohomeMount[] = "Mount";
const char kCryptohomeMountGuest[] = "MountGuest";
const char kCryptohomeMountPublic[] = "MountPublic";
const char kCryptohomeUnmount[] = "Unmount";
const char kCryptohomeTpmIsReady[] = "TpmIsReady";
const char kCryptohomeTpmIsEnabled[] = "TpmIsEnabled";
const char kCryptohomeTpmIsOwned[] = "TpmIsOwned";
const char kCryptohomeTpmIsBeingOwned[] = "TpmIsBeingOwned";
const char kCryptohomeTpmGetPassword[] = "TpmGetPassword";
const char kCryptohomeTpmCanAttemptOwnership[] = "TpmCanAttemptOwnership";
const char kCryptohomeTpmClearStoredPassword[] = "TpmClearStoredPassword";
const char kCryptohomePkcs11GetTpmTokenInfo[] = "Pkcs11GetTpmTokenInfo";
const char kCryptohomePkcs11GetTpmTokenInfoForUser[] =
    "Pkcs11GetTpmTokenInfoForUser";
const char kCryptohomePkcs11IsTpmTokenReady[] = "Pkcs11IsTpmTokenReady";
const char kCryptohomePkcs11IsTpmTokenReadyForUser[] =
    "Pkcs11IsTpmTokenReadyForUser";
const char kCryptohomeAsyncCheckKey[] = "AsyncCheckKey";
const char kCryptohomeAsyncMigrateKey[] = "AsyncMigrateKey";
const char kCryptohomeAsyncMount[] = "AsyncMount";
const char kCryptohomeAsyncMountGuest[] = "AsyncMountGuest";
const char kCryptohomeAsyncMountPublic[] = "AsyncMountPublic";
const char kCryptohomeAsyncRemove[] = "AsyncRemove";
const char kCryptohomeGetStatusString[] = "GetStatusString";
const char kCryptohomeRemoveTrackedSubdirectories[] =
    "RemoveTrackedSubdirectories";
const char kCryptohomeAsyncRemoveTrackedSubdirectories[] =
    "AsyncRemoveTrackedSubdirectories";
const char kCryptohomeDoAutomaticFreeDiskSpaceControl[] =
    "DoAutomaticFreeDiskSpaceControl";
const char kCryptohomeAsyncDoAutomaticFreeDiskSpaceControl[] =
    "AsyncDoAutomaticFreeDiskSpaceControl";
const char kCryptohomeAsyncDoesUsersExist[] = "AsyncDoesUsersExist";
const char kCryptohomeInstallAttributesGet[] = "InstallAttributesGet";
const char kCryptohomeInstallAttributesSet[] = "InstallAttributesSet";
const char kCryptohomeInstallAttributesCount[] = "InstallAttributesCount";
const char kCryptohomeInstallAttributesFinalize[] =
    "InstallAttributesFinalize";
const char kCryptohomeInstallAttributesIsReady[] = "InstallAttributesIsReady";
const char kCryptohomeInstallAttributesIsSecure[] =
    "InstallAttributesIsSecure";
const char kCryptohomeInstallAttributesIsInvalid[] =
    "InstallAttributesIsInvalid";
const char kCryptohomeInstallAttributesIsFirstInstall[] =
    "InstallAttributesIsFirstInstall";
const char kCryptohomeStoreEnrollmentState[] =
    "StoreEnrollmentState";
const char kCryptohomeLoadEnrollmentState[] =
    "LoadEnrollmentState";
const char kCryptohomeTpmIsAttestationPrepared[] = "TpmIsAttestationPrepared";
const char kCryptohomeTpmIsAttestationEnrolled[] = "TpmIsAttestationEnrolled";
const char kCryptohomeAsyncTpmAttestationCreateEnrollRequest[] =
    "AsyncTpmAttestationCreateEnrollRequest";
const char kCryptohomeAsyncTpmAttestationCreateEnrollRequestNew[] =
    "AsyncTpmAttestationCreateEnrollRequestNew";
const char kCryptohomeAsyncTpmAttestationEnroll[] = "AsyncTpmAttestationEnroll";
const char kCryptohomeAsyncTpmAttestationEnrollNew[] =
    "AsyncTpmAttestationEnrollNew";
const char kCryptohomeAsyncTpmAttestationCreateCertRequest[] =
    "AsyncTpmAttestationCreateCertRequest";
const char kCryptohomeAsyncTpmAttestationCreateCertRequestByProfile[] =
    "AsyncTpmAttestationCreateCertRequestByProfile";
const char kCryptohomeAsyncTpmAttestationFinishCertRequest[] =
    "AsyncTpmAttestationFinishCertRequest";
const char kCryptohomeTpmAttestationDoesKeyExist[] =
    "TpmAttestationDoesKeyExist";
const char kCryptohomeTpmAttestationGetCertificate[] =
    "TpmAttestationGetCertificate";
const char kCryptohomeTpmAttestationGetPublicKey[] =
    "TpmAttestationGetPublicKey";
const char kCryptohomeTpmAttestationRegisterKey[] = "TpmAttestationRegisterKey";
const char kCryptohomeTpmAttestationSignEnterpriseChallenge[] =
    "TpmAttestationSignEnterpriseChallenge";
const char kCryptohomeTpmAttestationSignSimpleChallenge[] =
    "TpmAttestationSignSimpleChallenge";
const char kCryptohomeTpmAttestationGetKeyPayload[] =
    "TpmAttestationGetKeyPayload";
const char kCryptohomeTpmAttestationSetKeyPayload[] =
    "TpmAttestationSetKeyPayload";
const char kCryptohomeTpmAttestationDeleteKeys[] =
    "TpmAttestationDeleteKeys";
const char kCryptohomeGetKeyDataEx[] = "GetKeyDataEx";
const char kCryptohomeCheckKeyEx[] = "CheckKeyEx";
const char kCryptohomeMountEx[] = "MountEx";
const char kCryptohomeAddKeyEx[] = "AddKeyEx";
const char kCryptohomeUpdateKeyEx[] = "UpdateKeyEx";
const char kCryptohomeRemoveKeyEx[] = "RemoveKeyEx";
const char kCryptohomeSignBootLockbox[] = "SignBootLockbox";
const char kCryptohomeVerifyBootLockbox[] = "VerifyBootLockbox";
const char kCryptohomeFinalizeBootLockbox[] = "FinalizeBootLockbox";
const char kCryptohomeGetBootAttribute[] = "GetBootAttribute";
const char kCryptohomeSetBootAttribute[] = "SetBootAttribute";
const char kCryptohomeFlushAndSignBootAttributes[] =
    "FlushAndSignBootAttributes";
const char kCryptohomeGetLoginStatus[] = "GetLoginStatus";
const char kCryptohomeGetTpmStatus[] = "GetTpmStatus";
const char kCryptohomeGetEndorsementInfo[] = "GetEndorsementInfo";
const char kCryptohomeRenameCryptohome[] = "RenameCryptohome";
const char kCryptohomeGetAccountDiskUsage[] = "GetAccountDiskUsage";
const char kCryptohomeGetFirmwareManagementParameters[] =
    "GetFirmwareManagementParameters";
const char kCryptohomeSetFirmwareManagementParameters[] =
    "SetFirmwareManagementParameters";
const char kCryptohomeRemoveFirmwareManagementParameters[] =
    "RemoveFirmwareManagementParameters";

// Signals
const char kSignalAsyncCallStatus[] = "AsyncCallStatus";
const char kSignalAsyncCallStatusWithData[] = "AsyncCallStatusWithData";
const char kSignalTpmInitStatus[] = "TpmInitStatus";
const char kSignalCleanupUsersRemoved[] = "CleanupUsersRemoved";
const char kSignalLowDiskSpace[] = "LowDiskSpace";
// Error code
enum MountError {
  MOUNT_ERROR_NONE = 0,
  MOUNT_ERROR_FATAL = 1 << 0,
  MOUNT_ERROR_KEY_FAILURE = 1 << 1,
  MOUNT_ERROR_MOUNT_POINT_BUSY = 1 << 2,
  MOUNT_ERROR_TPM_COMM_ERROR = 1 << 3,
  MOUNT_ERROR_TPM_DEFEND_LOCK = 1 << 4,
  MOUNT_ERROR_USER_DOES_NOT_EXIST = 1 << 5,
  MOUNT_ERROR_TPM_NEEDS_REBOOT = 1 << 6,
  MOUNT_ERROR_RECREATED = 1 << 31,
};
}  // namespace cryptohome

#endif  // SYSTEM_API_DBUS_CRYPTOHOME_DBUS_CONSTANTS_H_
