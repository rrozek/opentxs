// Copyright (c) 2018 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "opentxs/api/client/Manager.hpp"
#include "opentxs/api/client/Workflow.hpp"
#include "opentxs/api/storage/Storage.hpp"
#include "opentxs/api/Core.hpp"
#include "opentxs/api/Endpoints.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/api/Wallet.hpp"
#include "opentxs/core/contract/UnitDefinition.hpp"
#include "opentxs/core/Identifier.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/PasswordPrompt.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/ListenCallback.hpp"
#include "opentxs/network/zeromq/FrameIterator.hpp"
#include "opentxs/network/zeromq/FrameSection.hpp"
#include "opentxs/network/zeromq/Frame.hpp"
#include "opentxs/network/zeromq/Message.hpp"
#include "opentxs/network/zeromq/Socket.hpp"
#include "opentxs/network/zeromq/SubscribeSocket.hpp"
#include "opentxs/ui/BalanceItem.hpp"

#include "BalanceItemBlank.hpp"

#include <atomic>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <thread>
#include <tuple>
#include <vector>

#include "AccountActivity.hpp"

#define OT_METHOD "opentxs::ui::implementation::AccountActivity::"

#if OT_QT
namespace opentxs::ui
{
QT_MODEL_WRAPPER(AccountActivityQt, AccountActivity)

int AccountActivityQt::balancePolarity() const
{
    return parent_->BalancePolarity();
}
QString AccountActivityQt::displayBalance() const
{
    return parent_->DisplayBalance().c_str();
}
}  // namespace opentxs::ui
#endif

namespace opentxs::ui::implementation
{
AccountActivity::AccountActivity(
    const api::client::Manager& api,
    const network::zeromq::PublishSocket& publisher,
    const identifier::Nym& nymID,
    const Identifier& accountID
#if OT_QT
    ,
    const bool qt,
    const RowCallbacks insertCallback,
    const RowCallbacks removeCallback
#endif
    )
    : AccountActivityList(
          api,
          publisher,
          nymID
#if OT_QT
          ,
          qt,
          insertCallback,
          removeCallback,
          Roles{{AccountActivityQt::IDRole, "id"},
                {AccountActivityQt::AmountPolarityRole, "amountpolarity"},
                {AccountActivityQt::ContactsRole, "contacts"},
                {AccountActivityQt::DisplayAmountRole, "displayamount"},
                {AccountActivityQt::MemoRole, "memo"},
                {AccountActivityQt::WorkflowRole, "workflow"},
                {AccountActivityQt::TextRole, "text"},
                {AccountActivityQt::TimestampRole, "timestamp"},
                {AccountActivityQt::TypeRole, "type"}},
          1
#endif
          )
    , listeners_({
          {api_.Endpoints().WorkflowAccountUpdate(),
           new MessageProcessor<AccountActivity>(
               &AccountActivity::process_workflow)},
          {api_.Endpoints().AccountUpdate(),
           new MessageProcessor<AccountActivity>(
               &AccountActivity::process_balance)},
      })
    , balance_(0)
    , account_id_(accountID)
    , contract_(nullptr)
{
    init();
    setup_listeners(listeners_);
    startup_.reset(new std::thread(&AccountActivity::startup, this));

    OT_ASSERT(startup_)
}

void AccountActivity::construct_row(
    const AccountActivityRowID& id,
    const AccountActivitySortKey& index,
    const CustomData& custom) const
{
    OT_ASSERT(2 == custom.size())

    items_[index].emplace(
        id,
        Factory::BalanceItem(
            *this,
            api_,
            publisher_,
            id,
            index,
            custom,
            primary_id_,
            account_id_));
    names_.emplace(id, index);
}

#if OT_QT
QVariant AccountActivity::data(const QModelIndex& index, int role) const
{
    const auto [valid, pRow] = check_index(index);

    if (false == valid) { return {}; }

    const auto& row = *pRow;

    switch (role) {
        case AccountActivityQt::IDRole: {
            return row.UUID().c_str();
        }
        case AccountActivityQt::AmountPolarityRole: {
            return polarity(row.Amount());
        }
        case AccountActivityQt::ContactsRole: {
            std::string contacts;
            auto contact = row.Contacts().cbegin();

            if (contact != row.Contacts().cend()) {
                contacts = *contact;
                while (++contact != row.Contacts().cend()) {
                    contacts += ", " + *contact;
                }
            }

            return contacts.c_str();
        }
        case AccountActivityQt::DisplayAmountRole: {
            return row.DisplayAmount().c_str();
        }
        case AccountActivityQt::MemoRole: {
            return row.Memo().c_str();
        }
        case AccountActivityQt::WorkflowRole: {
            return row.Workflow().c_str();
        }
        case AccountActivityQt::TextRole: {
            return row.Text().c_str();
        }
        case AccountActivityQt::TimestampRole: {
            QDateTime qdatetime;
            qdatetime.setSecsSinceEpoch(
                std::chrono::system_clock::to_time_t(row.Timestamp()));

            return qdatetime;
        }
        case AccountActivityQt::TypeRole: {
            return static_cast<int>(row.Type());
        }
        default: {
            return {};
        }
    }

    return {};
}
#endif

std::string AccountActivity::DisplayBalance() const
{
    sLock lock(shared_lock_);

    if (contract_) {
        const auto amount = balance_.load();
        std::string output{};
        const auto formatted =
            contract_->FormatAmountLocale(amount, output, ",", ".");

        if (formatted) { return output; }

        return std::to_string(amount);
    }

    return {};
}

AccountActivity::EventRow AccountActivity::extract_event(
    const proto::PaymentEventType eventType,
    const proto::PaymentWorkflow& workflow)
{
    bool success{false};
    bool found{false};
    EventRow output{};
    auto& [time, event_p] = output;

    for (const auto& event : workflow.event()) {
        const auto eventTime =
            std::chrono::system_clock::from_time_t(event.time());

        if (eventType != event.type()) { continue; }

        if (eventTime > time) {
            if (success) {
                if (event.success()) {
                    time = eventTime;
                    event_p = &event;
                    found = true;
                }
            } else {
                time = eventTime;
                event_p = &event;
                success = event.success();
                found = true;
            }
        } else {
            if (false == success) {
                if (event.success()) {
                    // This is a weird case. It probably shouldn't happen
                    time = eventTime;
                    event_p = &event;
                    success = true;
                    found = true;
                }
            }
        }
    }

    if (false == found) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Workflow ")(workflow.id())(
            ", type ")(workflow.type())(", state ")(workflow.state())(
            " does not contain an event of type ")(eventType)
            .Flush();

        OT_FAIL;
    }

    return output;
}

std::vector<AccountActivity::RowKey> AccountActivity::extract_rows(
    const proto::PaymentWorkflow& workflow)
{
    std::vector<AccountActivity::RowKey> output;

    switch (workflow.type()) {
        case proto::PAYMENTWORKFLOWTYPE_OUTGOINGCHEQUE: {
            switch (workflow.state()) {
                case proto::PAYMENTWORKFLOWSTATE_UNSENT:
                case proto::PAYMENTWORKFLOWSTATE_CONVEYED:
                case proto::PAYMENTWORKFLOWSTATE_EXPIRED: {
                    output.emplace_back(
                        proto::PAYMENTEVENTTYPE_CREATE,
                        extract_event(
                            proto::PAYMENTEVENTTYPE_CREATE, workflow));
                } break;
                case proto::PAYMENTWORKFLOWSTATE_CANCELLED: {
                    output.emplace_back(
                        proto::PAYMENTEVENTTYPE_CREATE,
                        extract_event(
                            proto::PAYMENTEVENTTYPE_CREATE, workflow));
                    output.emplace_back(
                        proto::PAYMENTEVENTTYPE_CANCEL,
                        extract_event(
                            proto::PAYMENTEVENTTYPE_CANCEL, workflow));
                } break;
                case proto::PAYMENTWORKFLOWSTATE_ACCEPTED:
                case proto::PAYMENTWORKFLOWSTATE_COMPLETED: {
                    output.emplace_back(
                        proto::PAYMENTEVENTTYPE_CREATE,
                        extract_event(
                            proto::PAYMENTEVENTTYPE_CREATE, workflow));
                    output.emplace_back(
                        proto::PAYMENTEVENTTYPE_ACCEPT,
                        extract_event(
                            proto::PAYMENTEVENTTYPE_ACCEPT, workflow));
                } break;
                case proto::PAYMENTWORKFLOWSTATE_ERROR:
                case proto::PAYMENTWORKFLOWSTATE_INITIATED:
                default: {
                    LogOutput(OT_METHOD)(__FUNCTION__)(
                        ": Invalid workflow state (")(workflow.state())(")")
                        .Flush();
                }
            }
        } break;
        case proto::PAYMENTWORKFLOWTYPE_INCOMINGCHEQUE: {
            switch (workflow.state()) {
                case proto::PAYMENTWORKFLOWSTATE_CONVEYED:
                case proto::PAYMENTWORKFLOWSTATE_EXPIRED:
                case proto::PAYMENTWORKFLOWSTATE_COMPLETED: {
                    output.emplace_back(
                        proto::PAYMENTEVENTTYPE_CONVEY,
                        extract_event(
                            proto::PAYMENTEVENTTYPE_CONVEY, workflow));
                } break;
                case proto::PAYMENTWORKFLOWSTATE_ERROR:
                case proto::PAYMENTWORKFLOWSTATE_UNSENT:
                case proto::PAYMENTWORKFLOWSTATE_CANCELLED:
                case proto::PAYMENTWORKFLOWSTATE_ACCEPTED:
                case proto::PAYMENTWORKFLOWSTATE_INITIATED:
                default: {
                    LogOutput(OT_METHOD)(__FUNCTION__)(
                        ": Invalid workflow state (")(workflow.state())(")")
                        .Flush();
                }
            }
        } break;
        case proto::PAYMENTWORKFLOWTYPE_OUTGOINGTRANSFER: {
            switch (workflow.state()) {
                case proto::PAYMENTWORKFLOWSTATE_ACKNOWLEDGED:
                case proto::PAYMENTWORKFLOWSTATE_ACCEPTED: {
                    output.emplace_back(
                        proto::PAYMENTEVENTTYPE_ACKNOWLEDGE,
                        extract_event(
                            proto::PAYMENTEVENTTYPE_ACKNOWLEDGE, workflow));
                } break;
                case proto::PAYMENTWORKFLOWSTATE_COMPLETED: {
                    output.emplace_back(
                        proto::PAYMENTEVENTTYPE_ACKNOWLEDGE,
                        extract_event(
                            proto::PAYMENTEVENTTYPE_ACKNOWLEDGE, workflow));
                    output.emplace_back(
                        proto::PAYMENTEVENTTYPE_COMPLETE,
                        extract_event(
                            proto::PAYMENTEVENTTYPE_COMPLETE, workflow));
                } break;
                case proto::PAYMENTWORKFLOWSTATE_INITIATED:
                case proto::PAYMENTWORKFLOWSTATE_ABORTED: {
                } break;
                case proto::PAYMENTWORKFLOWSTATE_ERROR:
                case proto::PAYMENTWORKFLOWSTATE_UNSENT:
                case proto::PAYMENTWORKFLOWSTATE_CONVEYED:
                case proto::PAYMENTWORKFLOWSTATE_CANCELLED:
                case proto::PAYMENTWORKFLOWSTATE_EXPIRED:
                default: {
                    LogOutput(OT_METHOD)(__FUNCTION__)(
                        ": Invalid workflow state (")(workflow.state())(")")
                        .Flush();
                }
            }
        } break;
        case proto::PAYMENTWORKFLOWTYPE_INCOMINGTRANSFER: {
            switch (workflow.state()) {
                case proto::PAYMENTWORKFLOWSTATE_CONVEYED: {
                    output.emplace_back(
                        proto::PAYMENTEVENTTYPE_CONVEY,
                        extract_event(
                            proto::PAYMENTEVENTTYPE_CONVEY, workflow));
                } break;
                case proto::PAYMENTWORKFLOWSTATE_COMPLETED: {
                    output.emplace_back(
                        proto::PAYMENTEVENTTYPE_CONVEY,
                        extract_event(
                            proto::PAYMENTEVENTTYPE_CONVEY, workflow));
                    output.emplace_back(
                        proto::PAYMENTEVENTTYPE_ACCEPT,
                        extract_event(
                            proto::PAYMENTEVENTTYPE_ACCEPT, workflow));
                } break;
                case proto::PAYMENTWORKFLOWSTATE_ERROR:
                case proto::PAYMENTWORKFLOWSTATE_UNSENT:
                case proto::PAYMENTWORKFLOWSTATE_CANCELLED:
                case proto::PAYMENTWORKFLOWSTATE_ACCEPTED:
                case proto::PAYMENTWORKFLOWSTATE_EXPIRED:
                case proto::PAYMENTWORKFLOWSTATE_INITIATED:
                case proto::PAYMENTWORKFLOWSTATE_ABORTED:
                case proto::PAYMENTWORKFLOWSTATE_ACKNOWLEDGED:
                default: {
                    LogOutput(OT_METHOD)(__FUNCTION__)(
                        ": Invalid workflow state (")(workflow.state())(")")
                        .Flush();
                }
            }
        } break;
        case proto::PAYMENTWORKFLOWTYPE_INTERNALTRANSFER: {
            switch (workflow.state()) {
                case proto::PAYMENTWORKFLOWSTATE_ACKNOWLEDGED:
                case proto::PAYMENTWORKFLOWSTATE_CONVEYED:
                case proto::PAYMENTWORKFLOWSTATE_ACCEPTED: {
                    output.emplace_back(
                        proto::PAYMENTEVENTTYPE_ACKNOWLEDGE,
                        extract_event(
                            proto::PAYMENTEVENTTYPE_ACKNOWLEDGE, workflow));
                } break;
                case proto::PAYMENTWORKFLOWSTATE_COMPLETED: {
                    output.emplace_back(
                        proto::PAYMENTEVENTTYPE_ACKNOWLEDGE,
                        extract_event(
                            proto::PAYMENTEVENTTYPE_ACKNOWLEDGE, workflow));
                    output.emplace_back(
                        proto::PAYMENTEVENTTYPE_COMPLETE,
                        extract_event(
                            proto::PAYMENTEVENTTYPE_COMPLETE, workflow));
                } break;
                case proto::PAYMENTWORKFLOWSTATE_INITIATED:
                case proto::PAYMENTWORKFLOWSTATE_ABORTED: {
                } break;
                case proto::PAYMENTWORKFLOWSTATE_ERROR:
                case proto::PAYMENTWORKFLOWSTATE_UNSENT:
                case proto::PAYMENTWORKFLOWSTATE_CANCELLED:
                case proto::PAYMENTWORKFLOWSTATE_EXPIRED:
                default: {
                    LogOutput(OT_METHOD)(__FUNCTION__)(
                        ": Invalid workflow state (")(workflow.state())(")")
                        .Flush();
                }
            }
        } break;
        case proto::PAYMENTWORKFLOWTYPE_ERROR:
        case proto::PAYMENTWORKFLOWTYPE_OUTGOINGINVOICE:
        case proto::PAYMENTWORKFLOWTYPE_INCOMINGINVOICE:
        default: {
            LogOutput(OT_METHOD)(__FUNCTION__)(": Unsupported workflow type (")(
                workflow.type())(")")
                .Flush();
        }
    }

    return output;
}

void AccountActivity::process_balance(
    const opentxs::network::zeromq::Message& message)
{
    wait_for_startup();

    OT_ASSERT(2 == message.Body().size())

    const auto accountID = Identifier::Factory(message.Body().at(0));

    if (account_id_ != accountID) { return; }

    const auto& balance = message.Body().at(1);

    OT_ASSERT(balance.size() == sizeof(Amount))

    balance_.store(*static_cast<const Amount*>(balance.data()));
    UpdateNotify();
}

void AccountActivity::process_workflow(
    const Identifier& workflowID,
    std::set<AccountActivityRowID>& active)
{
    const auto workflow = api_.Workflow().LoadWorkflow(primary_id_, workflowID);

    OT_ASSERT(workflow)

    const auto rows = extract_rows(*workflow);

    for (const auto& [type, row] : rows) {
        const auto& [time, event_p] = row;
        AccountActivityRowID key{Identifier::Factory(workflowID), type};
        CustomData custom{new proto::PaymentWorkflow(*workflow),
                          new proto::PaymentEvent(*event_p)};
        add_item(key, time, custom);
        active.emplace(std::move(key));
    }
}

void AccountActivity::process_workflow(const network::zeromq::Message& message)
{
    wait_for_startup();

    OT_ASSERT(1 == message.Body().size());

    const std::string id(*message.Body().begin());
    const auto accountID = Identifier::Factory(id);

    OT_ASSERT(false == accountID->empty())

    if (account_id_ == accountID) { startup(); }
}

void AccountActivity::startup()
{
    auto reason = api_.Factory().PasswordPrompt("Loading account activity");
    auto account = api_.Wallet().Account(account_id_, reason);

    if (account) {
        balance_.store(account.get().GetBalance());
        UpdateNotify();
        eLock lock(shared_lock_);
        contract_ = api_.Wallet().UnitDefinition(
            api_.Storage().AccountContract(account_id_), reason);
    }

    account.Release();
    const auto workflows =
        api_.Workflow().WorkflowsByAccount(primary_id_, account_id_);
    std::set<AccountActivityRowID> active{};

    for (const auto& id : workflows) { process_workflow(id, active); }

    delete_inactive(active);
    startup_complete_->On();
}

AccountActivity::~AccountActivity()
{
    for (auto& it : listeners_) { delete it.second; }
}
}  // namespace opentxs::ui::implementation
