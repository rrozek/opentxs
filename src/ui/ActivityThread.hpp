// Copyright (c) 2018 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Internal.hpp"

#include "opentxs/api/client/OTX.hpp"
#include "opentxs/ui/ActivityThread.hpp"

#include "core/StateMachine.hpp"
#include "internal/ui/UI.hpp"
#include "List.hpp"

#include <future>

namespace std
{
using STORAGEID = std::
    tuple<opentxs::OTIdentifier, opentxs::StorageBox, opentxs::OTIdentifier>;

template <>
struct less<STORAGEID> {
    bool operator()(const STORAGEID& lhs, const STORAGEID& rhs) const
    {
        /* TODO: these lines will cause a segfault in the clang-5 ast parser.
                const auto & [ lID, lBox, lAccount ] = lhs;
                const auto & [ rID, rBox, rAccount ] = rhs;
        */
        const auto& lID = std::get<0>(lhs);
        const auto& lBox = std::get<1>(lhs);
        const auto& lAccount = std::get<2>(lhs);
        const auto& rID = std::get<0>(rhs);
        const auto& rBox = std::get<1>(rhs);
        const auto& rAccount = std::get<2>(rhs);

        if (lID->str() < rID->str()) { return true; }

        if (rID->str() < lID->str()) { return false; }

        if (lBox < rBox) { return true; }

        if (rBox < lBox) { return false; }

        if (lAccount->str() < rAccount->str()) { return true; }

        return false;
    }
};
}  // namespace std

namespace opentxs
{
using DraftTask = std::pair<
    ui::implementation::ActivityThreadRowID,
    api::client::OTX::BackgroundTask>;

template <>
struct make_blank<DraftTask> {
    static DraftTask value()
    {
        return {make_blank<ui::implementation::ActivityThreadRowID>::value(),
                make_blank<api::client::OTX::BackgroundTask>::value()};
    }
};
}  // namespace opentxs

namespace opentxs::ui::implementation
{
using ActivityThreadList = List<
    ActivityThreadExternalInterface,
    ActivityThreadInternalInterface,
    ActivityThreadRowID,
    ActivityThreadRowInterface,
    ActivityThreadRowInternal,
    ActivityThreadRowBlank,
    ActivityThreadSortKey,
    ActivityThreadPrimaryID>;

class ActivityThread final : public ActivityThreadList,
                             public opentxs::internal::StateMachine
{
public:
#if OT_QT
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole)
        const override;
#endif
    std::string DisplayName() const override;
    std::string GetDraft() const override;
    std::string Participants() const override;
    bool Pay(
        const std::string& amount,
        const Identifier& sourceAccount,
        const std::string& memo,
        const PaymentType type) const override;
    bool Pay(
        const Amount amount,
        const Identifier& sourceAccount,
        const std::string& memo,
        const PaymentType type) const override;
    std::string PaymentCode(
        const proto::ContactItemType currency) const override;
    bool same(const ActivityThreadRowID& lhs, const ActivityThreadRowID& rhs)
        const override;
    bool SendDraft() const override;
    bool SetDraft(const std::string& draft) const override;
    std::string ThreadID() const override;

    ~ActivityThread();

private:
    friend api::client::implementation::UI;

    const ListenerDefinitions listeners_;
    const OTIdentifier threadID_;
    std::set<OTIdentifier> participants_;
    mutable std::promise<void> participants_promise_;
    mutable std::shared_future<void> participants_future_;
    mutable std::mutex contact_lock_;
    mutable std::string draft_{""};
    mutable std::vector<DraftTask> draft_tasks_;
    std::shared_ptr<const opentxs::Contact> contact_;
    std::unique_ptr<std::thread> contact_thread_{nullptr};

    std::string comma(const std::set<std::string>& list) const;
    void can_message() const;
    void construct_row(
        const ActivityThreadRowID& id,
        const ActivityThreadSortKey& index,
        const CustomData& custom) const override;
    bool send_cheque(
        const PasswordPrompt& reason,
        const Amount amount,
        const Identifier& sourceAccount,
        const std::string& memo) const;
    bool validate_account(const Identifier& sourceAccount) const;

    void init_contact();
    void load_thread(const proto::StorageThread& thread);
    void new_thread();
    ActivityThreadRowID process_item(const proto::StorageThreadItem& item);
    bool process_drafts();
    void process_thread(const network::zeromq::Message& message);
    void startup();

    ActivityThread(
        const api::client::Manager& api,
        const network::zeromq::PublishSocket& publisher,
        const identifier::Nym& nymID,
        const Identifier& threadID
#if OT_QT
        ,
        const bool qt,
        const RowCallbacks insertCallback,
        const RowCallbacks removeCallback
#endif
    );

    ActivityThread() = delete;
    ActivityThread(const ActivityThread&) = delete;
    ActivityThread(ActivityThread&&) = delete;
    ActivityThread& operator=(const ActivityThread&) = delete;
    ActivityThread& operator=(ActivityThread&&) = delete;
};
}  // namespace opentxs::ui::implementation
