// Copyright (c) 2018 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENTXS_UI_ACCOUNTLISTITEM_HPP
#define OPENTXS_UI_ACCOUNTLISTITEM_HPP

#include "opentxs/Forward.hpp"

#include <string>

#include "ListRow.hpp"

#ifdef SWIG
// clang-format off
%extend opentxs::ui::AccountListItem {
    int Unit() const
    {
        return static_cast<int>($self->Unit());
    }
}
%ignore opentxs::ui::AccountListItem::Unit;
%template(OTUIAccountListItem) opentxs::SharedPimpl<opentxs::ui::AccountListItem>;
%rename(UIAccountListItem) opentxs::ui::AccountListItem;
// clang-format on
#endif  // SWIG

namespace opentxs
{
namespace ui
{
class AccountListItem : virtual public ListRow
{
public:
    EXPORT virtual std::string AccountID() const = 0;
    EXPORT virtual Amount Balance() const = 0;
    EXPORT virtual std::string ContractID() const = 0;
    EXPORT virtual std::string DisplayBalance() const = 0;
    EXPORT virtual std::string DisplayUnit() const = 0;
    EXPORT virtual std::string Name() const = 0;
    EXPORT virtual std::string NotaryID() const = 0;
    EXPORT virtual std::string NotaryName() const = 0;
    EXPORT virtual AccountType Type() const = 0;
    EXPORT virtual proto::ContactItemType Unit() const = 0;

    EXPORT virtual ~AccountListItem() = default;

protected:
    AccountListItem() = default;

private:
    AccountListItem(const AccountListItem&) = delete;
    AccountListItem(AccountListItem&&) = delete;
    AccountListItem& operator=(const AccountListItem&) = delete;
    AccountListItem& operator=(AccountListItem&&) = delete;
};
}  // namespace ui
}  // namespace opentxs
#endif
