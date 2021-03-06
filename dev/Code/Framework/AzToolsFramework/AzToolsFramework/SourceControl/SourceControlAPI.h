/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#pragma once

#include <AzCore/base.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/std/string/string.h>

namespace AzToolsFramework
{
    enum SourceControlStatus
    {
        SCS_ProviderIsDown,     // source control provider is down
        SCS_CertificateInvalid, // Trust certificate is invalid
        SCS_ProviderError,      // there was an error processing your request
        SCS_NUM_ERRORS,         // add errors above this enum

        SCS_Tracked,            // file is under source control
        SCS_NotTracked,         // source control unaware of file
        SCS_OpenByUser,         // currently open for checkout / staging
        SCS_NUM_STATUSES,       // add success statuses above this enum
    };

    enum SourceControlFlags
    {
        SCF_OutOfDate       = (1 << 0), // the file was out of date
        SCF_Writeable       = (1 << 1), // the file is writable on disk
        SCF_MultiCheckOut   = (1 << 2), // this file allows multiple owners
        SCF_OtherOpen       = (1 << 3), // someone else has this file open
        SCF_PendingAdd      = (1 << 4), // file marked for add
        SCF_PendingDelete   = (1 << 5), // file marked for removal
    };

    struct SourceControlFileInfo
    {
        SourceControlStatus m_status;
        unsigned int m_flags;
        AZStd::string m_filePath;
        AZStd::string m_StatusUser; // informational  -this is the user that caused the above status. (eg Checked Out -> who checked it out.  Out of date -> who submitted a new version.
        // secondary use of m_StatusUser is to signify that this file is checked out by other people than you, simultaneously

        SourceControlFileInfo()
            : m_status(SCS_ProviderIsDown)
            , m_flags((SourceControlFlags)0)
        {
        }

        SourceControlFileInfo(const char* fullFilePath)
            : m_status(SCS_ProviderIsDown)
            , m_filePath(fullFilePath)
            , m_flags((SourceControlFlags)0)
        {
        }

        bool CompareStatus(SourceControlStatus status) const { return m_status == status; }
        bool IsReadOnly() const { return !HasFlag(SCF_Writeable); }
        bool IsLockedByOther() const { return HasFlag(SCF_OtherOpen) && !HasFlag(SCF_MultiCheckOut); }

        bool HasFlag(SourceControlFlags flag) const { return ((m_flags & flag) != 0); }
    };

    // use bind if you need additional context.
    typedef AZStd::function<void(bool success, const SourceControlFileInfo& info)> SourceControlResponseCallback;

    enum class SourceControlSettingStatus : int
    {
        Invalid,

        PERFORCE_BEGIN,
        Unset,
        None,
        Set,
        Config,
        PERFORCE_END,
    };

    struct SourceControlSettingInfo
    {
        SourceControlSettingStatus m_status = SourceControlSettingStatus::Invalid;
        AZStd::string m_value;
        AZStd::string m_context;

        SourceControlSettingInfo() = default;

        //! is this value acutally present and usable?
        bool IsAvailable() const
        {
            return (m_status != SourceControlSettingStatus::Invalid) && (m_status != SourceControlSettingStatus::Unset) && (!m_value.empty());
        }

        //! Are we able to actually change this value without messing with global env or registry?
        bool IsSettable() const
        {
            if (m_status == SourceControlSettingStatus::Invalid)
            {
                return false;
            }

            return ((m_status == SourceControlSettingStatus::Unset) || (m_status == SourceControlSettingStatus::Set));
        }
    };

    typedef AZStd::function<void(const SourceControlSettingInfo& info)> SourceControlSettingCallback;

    enum class SourceControlState : int
    {
        Disabled,
        ConfigurationInvalid,
        Active,
    };

    //! SourceControlCommands
    //! This bus handles messages relating to source control commands
    //! source control commands are ASYNCHRONOUS
    //! do not block the main thread waiting for a response, it is not okay
    //! you will not get a message delivered unless you tick the tickbus anyway!
    class SourceControlCommands
        : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single; // there's only one source control listener right now
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;  // theres only one source control listener right now
        virtual ~SourceControlCommands() {}

        //! Get information on the file state
        virtual void GetFileInfo(const char* fullFilePath, const SourceControlResponseCallback& respCallback) = 0;

        //! Attempt to make a file ready for editing
        virtual void RequestEdit(const char* fullFilePath, bool allowMultiCheckout, const SourceControlResponseCallback& respCallback) = 0;

        //! Attempt to delete a file
        virtual void RequestDelete(const char* fullFilePath, const SourceControlResponseCallback& respCallback) = 0;

        //! Attempt to revert a file
        virtual void RequestRevert(const char* fullFilePath, const SourceControlResponseCallback& respCallback) = 0;
    };

    using SourceControlCommandBus = AZ::EBus<SourceControlCommands>;

    //! SourceControlConnectionRequests
    //! This bus handles messages relating to source control connectivity
    class SourceControlConnectionRequests
        : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        virtual ~SourceControlConnectionRequests() {}

        //! Suspend / Resume source control operations
        virtual void EnableSourceControl(bool enable) = 0;

        //! Returns if source control operations are enabled
        virtual bool IsActive() const = 0;

        //! Enable or disable trust of an SSL connection
        virtual void EnableTrust(bool enable, AZStd::string fingerprint) = 0;

        //! Returns if we are connecting to an SSL server without a valid key
        virtual bool HasTrustIssue() const = 0;

        //! Attempt to set connection setting 'key' to 'value'
        virtual void SetConnectionSetting(const char* key, const char* value, const SourceControlSettingCallback& respCallBack) = 0;

        //! Attempt to get connection setting by key
        virtual void GetConnectionSetting(const char* key, const SourceControlSettingCallback& respCallBack) = 0;

        //! Returns if source control is disabled, has invalid configurations, or enabled
        virtual SourceControlState GetSourceControlState() const { return SourceControlState::Disabled; }
    };

    using SourceControlConnectionRequestBus = AZ::EBus<SourceControlConnectionRequests>;

    //! SourceControlNotifications
    //! Outgoing messages from source control
    class SourceControlNotifications
        : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        virtual ~SourceControlNotifications() {}

        //! Request to trust source control key with provided fingerprint
        virtual void RequestTrust(const char* /*fingerprint*/) {}

        //! Notify listeners that our connectivity state has changed
        virtual void ConnectivityStateChanged(const SourceControlState /*connected*/) {}
    };

    using SourceControlNotificationBus = AZ::EBus<SourceControlNotifications>;
}; // namespace AzToolsFramework
