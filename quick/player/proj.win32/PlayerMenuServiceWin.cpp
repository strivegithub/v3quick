
#include "PlayerMenuServiceWin.h"

PLAYER_NS_BEGIN

PlayerMenuItemWin *PlayerMenuItemWin::create(const string &menuId, const string &title)
{
    PlayerMenuItemWin *item = new PlayerMenuItemWin();
    item->_menuId = menuId;
    item->_title = title;
    item->autorelease();
    return item;
}

PlayerMenuItemWin::PlayerMenuItemWin()
    : _parent(nullptr)
    , _commandId(0)
    , _hmenu(NULL)
{
}

PlayerMenuItemWin::~PlayerMenuItemWin()
{
    CC_SAFE_RELEASE(_parent);
    if (_hmenu)
    {
        CCLOG("PlayerMenuItemWin::~PlayerMenuItemWin() - %s (HMENU)", _menuId.c_str());
        DestroyMenu(_hmenu);
    }
    else
    {
        CCLOG("PlayerMenuItemWin::~PlayerMenuItemWin() - %s", _menuId.c_str());
    }
}

void PlayerMenuItemWin::setTitle(const string &title)
{
    if (title.length() == 0)
    {
        CCLOG("MenuServiceWin::setTitle() - can not set menu title to empty, menu id (%s)", _menuId.c_str());
        return;
    }

    MENUITEMINFO menuitem;
    menuitem.cbSize = sizeof(menuitem);
    menuitem.fMask = MIIM_FTYPE | MIIM_STRING;
    menuitem.fType = (title.compare("-") == 0) ? MFT_SEPARATOR : MFT_STRING;
    u16string u16title;
    StringUtils::UTF8ToUTF16(title, u16title);
    menuitem.dwTypeData = (LPTSTR)u16title.c_str();
    if (SetMenuItemInfo(_parent->_hmenu, _commandId, MF_BYCOMMAND, &menuitem))
    {
        _title = title;
    }
    else
    {
        DWORD err = GetLastError();
        CCLOG("MenuServiceWin::setTitle() - set menu title failed, menu id (%s). error code = %u", _menuId.c_str(), err);
    }
}

void PlayerMenuItemWin::setEnabled(bool enabled)
{
    MENUITEMINFO menuitem;
    menuitem.cbSize = sizeof(menuitem);
    menuitem.fMask = MIIM_STATE;
    menuitem.fState = (enabled) ? MFS_ENABLED : MFS_DISABLED;
    if (SetMenuItemInfo(_parent->_hmenu, _commandId, MF_BYCOMMAND, &menuitem))
    {
        _isEnabled = enabled;
    }
    else
    {
        DWORD err = GetLastError();
        CCLOG("MenuServiceWin::setEnabled() - set menu enabled failed, menu id (%s). error code = %u", _menuId.c_str(), err);
    }
}

void PlayerMenuItemWin::setChecked(bool checked)
{
    MENUITEMINFO menuitem;
    menuitem.cbSize = sizeof(menuitem);
    menuitem.fMask = MIIM_STATE;
    menuitem.fState = (checked) ? MFS_CHECKED : MFS_UNCHECKED;
    if (SetMenuItemInfo(_parent->_hmenu, _commandId, MF_BYCOMMAND, &menuitem))
    {
        _isChecked = checked;
    }
    else
    {
        DWORD err = GetLastError();
        CCLOG("MenuServiceWin::setChecked() - set menu checked failed, menu id (%s). error code = %u", _menuId.c_str(), err);
    }
}

void PlayerMenuItemWin::setShortcut(const string &shortcut)
{

}

// MenuServiceWin

UINT PlayerMenuServiceWin::_newCommandId = 0x1000;

PlayerMenuServiceWin::PlayerMenuServiceWin(HWND hwnd)
    : _hwnd(hwnd)
{
    // create menu
    _root._commandId = 0;
    _root._hmenu = CreateMenu();
    SetMenu(hwnd, _root._hmenu);
}

PlayerMenuServiceWin::~PlayerMenuServiceWin()
{
}

PlayerMenuItem *PlayerMenuServiceWin::addItem(const string &menuId, const string &title, const string &parentId, int order /* = MAX_ORDER */)
{
    if (menuId.length() == 0 || title.length() == 0)
    {
        CCLOG("MenuServiceWin::addItem() - menuId and title must is non-empty");
        return nullptr;
    }

    // check menu id is exists
    if (_items.find(menuId) != _items.end())
    {
        CCLOG("MenuServiceWin::addItem() - menu id (%s) is exists", menuId.c_str());
        return false;
    }

    // set parent
    PlayerMenuItemWin *parent = &_root;
    if (parentId.length())
    {
        // query parent menu
        auto it = _items.find(parentId);
        if (it != _items.end())
        {
            parent = it->second;
        }
    }

    if (!parent->_hmenu)
    {
        // create menu handle for parent (convert parent to submenu)
        parent->_hmenu = CreateMenu();
        parent->_isGroup = true;
        MENUITEMINFO menuitem;
        menuitem.cbSize = sizeof(menuitem);
        menuitem.fMask = MIIM_SUBMENU;
        menuitem.hSubMenu = parent->_hmenu;
        if (!SetMenuItemInfo(parent->_parent->_hmenu, parent->_commandId, MF_BYCOMMAND, &menuitem))
        {
            DWORD err = GetLastError();
            CCLOG("MenuServiceWin::addItem() - set menu handle failed, menu id (%s). error code = %u", parent->_menuId.c_str(), err);
            return nullptr;
        }
    }

    // create new menu item
    _newCommandId++;
    PlayerMenuItemWin *item = PlayerMenuItemWin::create(menuId, title);
    item->_commandId = _newCommandId;
    item->_parent = parent;
    item->_parent->retain();

    // add menu item to menu bar
    MENUITEMINFO menuitem;
    menuitem.cbSize = sizeof(menuitem);
    menuitem.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STATE | MIIM_STRING;
    menuitem.fType = (item->_title.compare("-") == 0) ? MFT_SEPARATOR : MFT_STRING;
    menuitem.fState = (item->_isEnabled) ? MFS_ENABLED : MFS_DISABLED;
    menuitem.fState |= (item->_isChecked) ? MFS_CHECKED : MFS_UNCHECKED;
    u16string u16title;
    StringUtils::UTF8ToUTF16(item->_title, u16title);
    menuitem.dwTypeData = (LPTSTR)u16title.c_str();
    menuitem.wID = _newCommandId;

    // check new menu item position
    if (order > parent->_children.size())
    {
        order = parent->_children.size();
    }
    else if (order < 0)
    {
        order = 0;
    }

    // create new menu item
    if (!InsertMenuItem(parent->_hmenu, order, TRUE, &menuitem))
    {
        DWORD err = GetLastError();
        CCLOG("MenuServiceWin::addItem() - insert new menu item failed, menu id (%s). error code = %u", item->_menuId.c_str(), err);
        item->release();
        return nullptr;
    }

    // update menu state
    parent->_children.insert(order, item);
    _items[item->_menuId] = item;
    _commandId2menuId[item->_commandId] = item->_menuId;

    return item;
}

PlayerMenuItem *PlayerMenuServiceWin::addItem(const string &menuId, const string &title)
{
    return addItem(menuId, title, "");
}

PlayerMenuItem *PlayerMenuServiceWin::getItem(const string &menuId)
{
    auto it = _items.find(menuId);
    if (it == _items.end())
    {
        CCLOG("MenuServiceWin::getItem() - Invalid menu id (%s)", menuId.c_str());
        return nullptr;
    }

    return it->second;
}

bool PlayerMenuServiceWin::removeItem(const string &menuId)
{
    auto it = _items.find(menuId);
    if (it == _items.end())
    {
        CCLOG("MenuServiceWin::removeItem() - Invalid menu id (%s)", menuId.c_str());
        return false;
    }

    PlayerMenuItemWin *item = it->second;
    if (item->_children.size() == 0)
    {
        if (!DeleteMenu(item->_parent->_hmenu, item->_commandId, MF_BYCOMMAND))
        {
            DWORD err = GetLastError();
            CCLOG("MenuServiceWin::removeItem() - remove menu item failed, menu id (%s). error code = %u", item->_menuId.c_str(), err);
            return false;
        }

        // remove item from parent
        bool removed = false;
        auto *children = &item->_parent->_children;
        for (auto it = children->begin(); it != children->end(); ++it)
        {
            if ((*it)->_commandId == item->_commandId)
            {
                CCLOG("MenuServiceWin::removeItem() - remove menu item (%s)", item->_menuId.c_str());
                children->erase(it);
                removed = true;
                break;
            }
        }

        if (!removed)
        {
            CCLOG("MenuServiceWin::removeItem() - remove menu item (%s) failed, not found command id from parent->children", item->_menuId.c_str());
        }

        // remove menu id mapping
        _items.erase(menuId);
        _commandId2menuId.erase(item->_commandId);
        DrawMenuBar(_hwnd);
        return true;
    }
    else
    {
        // remove all children
        while (item->_children.size() != 0)
        {
            PlayerMenuItemWin *child = *item->_children.begin();
            if (!removeItem(child->_menuId.c_str()))
            {
                break;
                return false;
            }
        }
        return removeItem(menuId);
    }

    return false;
}

PLAYER_NS_END
