// Copyright (c) 2018 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Internal.hpp"

#include "opentxs/core/identifier/Server.hpp"

#include "internal/consensus/Consensus.hpp"

#include <atomic>
#include <cstdint>

namespace opentxs::implementation
{
class Context : virtual public internal::Context
{
public:
    std::set<RequestNumber> AcknowledgedNumbers() const override;
    const api::Core& Api() const override { return api_; }
    std::size_t AvailableNumbers() const override;
    bool HaveLocalNymboxHash() const override;
    bool HaveRemoteNymboxHash() const override;
    std::set<TransactionNumber> IssuedNumbers() const override;
    std::string Name() const override;
    bool NymboxHashMatch() const override;
    std::string LegacyDataFolder() const override;
    OTIdentifier LocalNymboxHash() const override;
    std::unique_ptr<const opentxs::NymFile> Nymfile(
        const PasswordPrompt& reason) const override;
    const identity::Nym& RemoteNym() const override;
    OTIdentifier RemoteNymboxHash() const override;
    RequestNumber Request() const override;
    OTData Serialize() const override;
    proto::Context Serialized() const override;
    const identifier::Server& Server() const override { return server_id_; }
    bool VerifyAcknowledgedNumber(const RequestNumber& req) const override;
    bool VerifyAvailableNumber(const TransactionNumber& number) const override;
    bool VerifyIssuedNumber(const TransactionNumber& number) const override;

    bool AddAcknowledgedNumber(const RequestNumber req) override;
    bool CloseCronItem(const TransactionNumber) override { return false; }
    bool ConsumeAvailable(const TransactionNumber& number) override;
    bool ConsumeIssued(const TransactionNumber& number) override;
    RequestNumber IncrementRequest() override;
    bool InitializeNymbox(const PasswordPrompt& reason) override;
    Editor<opentxs::NymFile> mutable_Nymfile(
        const PasswordPrompt& reason) override;
    bool OpenCronItem(const TransactionNumber) override { return false; }
    bool RecoverAvailableNumber(const TransactionNumber& number) override;
    proto::Context Refresh(const PasswordPrompt& reason) override;
    bool RemoveAcknowledgedNumber(const std::set<RequestNumber>& req) override;
    void Reset() override;
    void SetLocalNymboxHash(const Identifier& hash) override;
    void SetRemoteNymboxHash(const Identifier& hash) override;
    void SetRequest(const RequestNumber req) override;

    virtual ~Context() = default;

protected:
    const api::Core& api_;
    const OTServerID server_id_;
    Nym_p remote_nym_{};
    std::set<TransactionNumber> available_transaction_numbers_{};
    std::set<TransactionNumber> issued_transaction_numbers_{};
    std::atomic<RequestNumber> request_number_{0};
    std::set<RequestNumber> acknowledged_request_numbers_{};
    OTIdentifier local_nymbox_hash_;
    OTIdentifier remote_nymbox_hash_;

    proto::Context contract(const Lock& lock) const;
    OTIdentifier GetID(const Lock& lock) const override;
    proto::Context serialize(const Lock& lock, const proto::ConsensusType type)
        const;
    virtual proto::Context serialize(const Lock& lock) const = 0;
    virtual std::string type() const = 0;
    bool validate(const Lock& lock, const PasswordPrompt& reason)
        const override;

    bool add_acknowledged_number(const Lock& lock, const RequestNumber req);
    bool consume_available(const Lock& lock, const TransactionNumber& number);
    bool consume_issued(const Lock& lock, const TransactionNumber& number);
    void finish_acknowledgements(
        const Lock& lock,
        const std::set<RequestNumber>& req);
    bool issue_number(const Lock& lock, const TransactionNumber& number);
    bool recover_available_number(
        const Lock& lock,
        const TransactionNumber& number);
    bool remove_acknowledged_number(
        const Lock& lock,
        const std::set<RequestNumber>& req);
    bool save(const Lock& lock, const PasswordPrompt& reason);
    void set_local_nymbox_hash(const Lock& lock, const Identifier& hash);
    void set_remote_nymbox_hash(const Lock& lock, const Identifier& hash);
    bool update_signature(const Lock& lock, const PasswordPrompt& reason)
        override;
    bool verify_available_number(const Lock& lock, const TransactionNumber& req)
        const;
    bool verify_acknowledged_number(const Lock& lock, const RequestNumber& req)
        const;
    bool verify_issued_number(const Lock& lock, const TransactionNumber& number)
        const;

    Context(
        const api::Core& api,
        const VersionNumber targetVersion,
        const Nym_p& local,
        const Nym_p& remote,
        const identifier::Server& server);
    Context(
        const api::Core& api,
        const VersionNumber targetVersion,
        const proto::Context& serialized,
        const Nym_p& local,
        const Nym_p& remote,
        const identifier::Server& server);

private:
    friend opentxs::Factory;

    const VersionNumber target_version_{0};

    virtual const identifier::Nym& client_nym_id(const Lock& lock) const = 0;
    proto::Context IDVersion(const Lock& lock) const;
    virtual const identifier::Nym& server_nym_id(const Lock& lock) const = 0;
    proto::Context SigVersion(const Lock& lock) const;
    bool verify_signature(
        const Lock& lock,
        const proto::Signature& signature,
        const PasswordPrompt& reason) const override;

    // Transition method used for converting from Nym class
    bool insert_available_number(const TransactionNumber& number);
    // Transition method used for converting from Nym class
    bool insert_issued_number(const TransactionNumber& number);

    Context() = delete;
    Context(const Context&) = delete;
    Context(Context&&) = delete;
    Context& operator=(const Context&) = delete;
    Context& operator=(Context&&) = delete;
};
}  // namespace opentxs::implementation
