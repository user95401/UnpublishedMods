#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

#define public_cast(value, member) [](auto* v) { \
	class FriendClass__; \
	using T = std::remove_pointer<decltype(v)>::type; \
	class FriendeeClass__: public T { \
	protected: \
		friend FriendClass__; \
	}; \
	class FriendClass__ { \
	public: \
		auto& get(FriendeeClass__* v) { return v->member; } \
	} c; \
	return c.get(reinterpret_cast<FriendeeClass__*>(v)); \
}(value)

class ModLogoLoader : public CCNode {
public:
    EventListener<web::WebTask> m_listener;
    Mod* m_mod;
    Ref<CCNode> m_node = nullptr;
    Ref<LoadingCircleSprite> m_loadinc = nullptr;
    int m_methodID = 0;
    auto static attach(Mod* mod, CCNode* node = nullptr, int methodID = 0) {
        auto loader = new ModLogoLoader;
        loader->init();
        loader->setID("loader"_spr);
        loader->m_mod = (mod);
        loader->m_node = node ? node : geode::createModLogo(mod);
        loader->m_methodID = (methodID);
        auto na_label = loader->m_node->getChildByID("sprite");
        if (typeinfo_cast<CCLabelBMFont*>(na_label)) {
            loader->m_loadinc = LoadingCircleSprite::create();
            loader->m_loadinc->setLayoutOptions(AnchorLayoutOptions::create()->setAnchor(Anchor::Center));
            loader->m_loadinc->fadeInCircle(0);
            loader->m_loadinc->setScale(0.7f);
            loader->m_node->addChild(loader->m_loadinc);
            loader->m_node->updateLayout();
            loader->request();
        }
        loader->m_node->addChild(loader);
        return loader->m_node;
    }
    void request() {
        auto repo = m_mod->getMetadata().getRepository().value_or("");
        auto raw = string::replace(repo,
            "https://github.com/", "https://raw.githubusercontent.com/"
        );
        auto url = std::string();
        if (m_methodID == 0) {
            url = raw + "/" + m_mod->getVersion().toString() + "/logo.png";
        }
        if (m_methodID == 1) {
            url = raw + "/main/logo.png";
        }
        if (m_methodID == 2) {
            url = raw + "/master/logo.png";
        }
        if (m_methodID == 3) {
            url = "https://api.geode-sdk.org/v1/mods/" + m_mod->getID() + "/logo";
        }
        m_listener.bind(
            [this](web::WebTask::Event* e) {
                if (web::WebResponse* res = e->getValue())
                    response(res);
            }
        );
        auto req = web::WebRequest();
        m_listener.setFilter(req.get(url));
    };
    void response(web::WebResponse* res) {
        if (this == nullptr) return;
        if (this->m_node == nullptr) return;
        auto& rtn = this->m_node;

        auto str = res->string().value_or("NO_DATA");
        str = res->string().error_or(str);

        if ((res->code() < 399) and (res->code() > 10)) {

            if (auto na_label = rtn->getChildByID("sprite"))
                na_label->removeFromParentAndCleanup(0);

            auto error_code = std::error_code();
            auto path = dirs::getTempDir() / (m_mod->getID() + ".temp.logo.png");

            std::ofstream(path, std::ios::binary) << str;
            auto sprite = CCSprite::create(path.string().c_str());
            std::filesystem::remove(path, error_code);

            if (dynamic_cast<CCSprite*>(sprite) and dynamic_cast<CCNode*>(sprite)) {
                sprite->setLayoutOptions(AnchorLayoutOptions::create()->setAnchor(Anchor::Center));
                sprite->setScale(rtn->getContentSize().width / sprite->getContentSize().width);
                rtn->addChild(sprite);
                rtn->updateLayout();
            };
        }
        else {
            log::error("code: {}, err: {}",
                res->code(), res->string().error_or(str)
            );
            m_methodID = m_methodID + 1;
            if (m_methodID <= 3) return request();
        }

        if (m_loadinc) m_loadinc->setVisible(0);
    }
};

static auto loaderMod = Loader::get()->getLoadedMod("geode.loader");

class ModPopup : public FLAlertLayer {};
class ModItem : public CCNode {};

class ModSource {};
class ModListSource {
public:
    struct LoadPageError {
        std::string message;
        std::optional<std::string> details;

        LoadPageError() = default;
        LoadPageError(std::string const& msg) : message(msg) {}
        LoadPageError(auto msg, auto details) : message(msg), details(details) {}
    };

    using Page = std::vector<Ref<ModItem>>;
    using PageLoadTask = Task<Result<Page, LoadPageError>, std::optional<uint8_t>>;

    struct ProvidedMods {
        std::vector<ModSource> mods;
        size_t totalModCount;
    };
    using ProviderTask = Task<Result<ProvidedMods, LoadPageError>, std::optional<uint8_t>>;

    std::unordered_map<size_t, Page> m_cachedPages;
    std::optional<size_t> m_cachedItemCount;

    virtual void resetQuery() = 0;
    virtual ProviderTask fetchPage(size_t page, size_t pageSize, bool forceUpdate) = 0;
    virtual void setSearchQuery(std::string const& query) = 0;

    ModListSource(ModListSource const&) = delete;
    ModListSource(ModListSource&&) = delete;

    virtual bool isDefaultQuery() const = 0;

    virtual std::unordered_set<std::string> getModTags() const = 0;
    virtual void setModTags(std::unordered_set<std::string> const& tags) = 0;

};
class ModList : public CCNode {
public:
    ModListSource* m_source;
    size_t m_page = 0;
    ScrollLayer* m_list;
    CCMenu* m_statusContainer;
    CCLabelBMFont* m_statusTitle;
    SimpleTextArea* m_statusDetails;
    CCMenuItemSpriteExtra* m_statusDetailsBtn;
    CCNode* m_statusLoadingCircle;
    Slider* m_statusLoadingBar;
    EventListener<ModListSource::PageLoadTask> m_listener;
    CCMenuItemSpriteExtra* m_pagePrevBtn;
    CCMenuItemSpriteExtra* m_pageNextBtn;
    CCNode* m_topContainer;
    CCNode* m_searchMenu;
    CCNode* m_updateAllContainer = nullptr;
    CCMenu* m_updateAllMenu = nullptr;
    IconButtonSprite* m_updateAllSpr = nullptr;
    CCMenuItemSpriteExtra* m_updateAllBtn = nullptr;
    CCNode* m_updateAllLoadingCircle = nullptr;
    IconButtonSprite* m_showUpdatesSpr = nullptr;
    IconButtonSprite* m_hideUpdatesSpr = nullptr;
    CCMenuItemToggler* m_toggleUpdatesOnlyBtn = nullptr;
    CCNode* m_errorsContainer = nullptr;
    CCMenuItemToggler* m_toggleErrorsOnlyBtn = nullptr;
    TextArea* m_updateCountLabel = nullptr;
    TextInput* m_searchInput;
    CCMenuItemSpriteExtra* m_filtersBtn;
    CCMenuItemSpriteExtra* m_clearFiltersBtn;
    /*
    EventListener<InvalidateCacheFilter> m_invalidateCacheListener;
    EventListener<server::ServerRequest<std::vector<std::string>>> m_checkUpdatesListener;
    EventListener<server::ModDownloadFilter> m_downloadListener;
    bool m_bigSize = false;
    std::atomic<size_t> m_searchInputThreads = 0;
    */
}; 
class ModsStatusNode : public CCNode {};
class UpdateModListStateFilter : public EventFilter<Event> {};
class ModsLayer : public CCLayer, public SetIDPopupDelegate {
public:
    CCNode* m_frame;
    std::vector<CCMenuItemSpriteExtra*> m_tabs;
    ModListSource* m_currentSource = nullptr;
    std::unordered_map<ModListSource*, Ref<ModList>> m_lists;
    CCMenu* m_pageMenu;
    CCLabelBMFont* m_pageLabel;
    CCMenuItemSpriteExtra* m_goToPageBtn;
    ModsStatusNode* m_statusNode;
    EventListener<UpdateModListStateFilter> m_updateStateListener;
    bool m_showSearch = false;
    bool m_bigView = false;
    void resetModsList() {
        if (auto customListData = typeinfo_cast<GJGameLevel*>(this->getChildByIDRecursive("customListData"_spr))) {
            auto parse_err = std::string();
            auto parse = matjson::parse(customListData->m_levelString.data(), parse_err);
            if (parse.has_value()) this->setupModsList(parse.value());
        }
    }
    void setupModsList(matjson::Value json) {
        //get nodes
        ModList* pModList = nullptr;
        CCMenuItemToggler* unverifiedModsToggle = nullptr;
        {
            if (pModList = typeinfo_cast<ModList*>(this->getChildByIDRecursive("ModList"))) void(); else
                return log::error("{}.ModList\" = {}", this, pModList);
            if (unverifiedModsToggle = typeinfo_cast<CCMenuItemToggler*>(this->getChildByIDRecursive("unverifiedModsToggle"_spr))) void(); else
                return log::error("{}.unverifiedModsToggle\" = {}", this, unverifiedModsToggle);
        };
        if (not unverifiedModsToggle->isOn()) return;

        auto mods = json["payload"]["data"].as_array();

        //go back if no mods
        if (mods.empty()) {
            pModList->m_pagePrevBtn->setVisible(1);
            pModList->m_pageNextBtn->setVisible(0);
            return pModList->m_pagePrevBtn->activate();
        }

        //upd page
        auto page = unverifiedModsToggle->getTag();
        this->m_pageLabel->setString(fmt::format("Page {}", page).data());
        this->m_pageLabel->getParent()->updateLayout();
        pModList->m_pagePrevBtn->setVisible(page > 1);
        pModList->m_pageNextBtn->setVisible(mods.size() == 10);

        //detect m_bigView...
        cocos::findFirstChildRecursive<ModItem>(this,
            [this](ModItem* node) {
                m_bigView = node->getContentHeight() >= 40.f;
                return true;
            }
        );

        //store json data..........
        pModList->removeChildByID("customListData"_spr);
        auto customListData = GJGameLevel::create();
        customListData->m_levelString = gd::string(json.dump(0).data());
        customListData->setID("customListData"_spr);
        pModList->addChild(customListData);

        //old pos
        auto customListPoint = CCPointZero;
        if (auto customList = typeinfo_cast<ScrollLayer*>(pModList->getChildByIDRecursive("customList"_spr))) {
            customListPoint = customList->m_contentLayer->getPosition();
        }

        pModList->removeChildByID("customList"_spr);

        auto customList = ScrollLayer::create(pModList->m_list->getContentSize());
        customList->setID("customList"_spr);
        customList->setAnchorPoint(CCPointZero);
        customList->m_contentLayer->setLayout(ColumnLayout::create()
            ->setAxisAlignment(AxisAlignment::End)
            ->setAxisReverse(true)
            ->setGap(2.500f)
        );
        
        pModList->addChild(customList);

        customList->m_contentLayer->setContentHeight(0.f);
        for (auto mod_entry : mods) {

            loaderMod->setLoggingEnabled(0);
            auto mod = loaderMod;
            {
                auto version = mod_entry["versions"][0];
                version["id"] = mod_entry["id"];
                version["tags"] = mod_entry["tags"];
                version["links"] = mod_entry["links"];
                version["developers"] = matjson::Array();
                for (auto dev : mod_entry["developers"].as_array()) {
                    version["developers"].as_array().push_back(dev["display_name"]);
                }
                auto mod_entry_repository = mod_entry.try_get<std::string>("repository");
                auto modmetaresult = ModMetadata::create(version);
                if (modmetaresult.has_value()) void();
                else return log::error("failed get mod meta: {}", modmetaresult.err());
                auto modmeta = modmetaresult.value();
                modmeta.setDetails(mod_entry.try_get<std::string>("about").value_or(""));
                modmeta.setChangelog(mod_entry.try_get<std::string>("changelog").value_or(""));
                if (not modmeta.getRepository().has_value() and mod_entry_repository.has_value()) {
                    modmeta.setRepository(mod_entry_repository.value());
                }
                mod = new Mod(modmeta);
            };
            loaderMod->setLoggingEnabled(1);

            auto menu = CCMenu::create();
            menu->setLayout(AnchorLayout::create());
            menu->setContentSize(
                { customList->m_contentLayer->getContentWidth(), m_bigView ? 40.f : 30.f }
            );

            auto bg = CCScale9Sprite::create("square02b_small.png");
            bg->setID("bg");
            if (loaderMod->getSettingValue<bool>("enable-geode-theme")) {
                bg->setOpacity(25);
                bg->setColor({ 255, 255, 255 });
            }
            else {
                bg->setOpacity(90);
                bg->setColor({ 0, 0, 0 });
            };
            bg->setContentHeight(menu->getContentHeight() - 0.f);
            bg->setContentWidth(menu->getContentWidth() - 6.f);
            bg->setLayoutOptions(AnchorLayoutOptions::create()
                ->setAnchor(Anchor::Center)
            );
            menu->addChild(bg);

            auto logo = ModLogoLoader::attach(mod);
            logo->setLayoutOptions(AnchorLayoutOptions::create()
                ->setAnchor(Anchor::Left)
                ->setOffset({ m_bigView ? 24.f : 20.f, 0.f })
            );
            logo->setID("logo");
            logo->setScale(m_bigView ? 0.6 : 0.375f);
            menu->addChild(logo);

            auto name_container = CCNode::create();
            name_container->setID("name_container");
            name_container->setAnchorPoint({ 0.f, 0.5f });
            name_container->setLayoutOptions(AnchorLayoutOptions::create()
                ->setAnchor(Anchor::Left)
                ->setOffset({ m_bigView ? 44.f : 40.f, 6.f })
            );
            name_container->setContentWidth(252.000f);

            auto ver = CCLabelBMFont::create(
                mod->getVersion().toString().data(), "bigFont.fnt"
            );
            ver->setID("ver");
            ver->setScale(0.3f);
            ver->setColor({ 112, 235, 41 });

            auto name = CCLabelBMFont::create(
                mod->getName().data(), "bigFont.fnt"
            );
            name->setID("name");
            name->setScale(0.375f);

            auto maxNameW = 710.f - ver->getContentWidth();
            auto ew = maxNameW / name->getContentWidth();
            auto ew2 = 1.f - ew;//imstupdo
            if (name->getContentWidth() >= maxNameW)
                name->setScale(name->getScale() - ew2);

            name_container->addChild(name);
            name_container->addChild(ver);

            name_container->setLayout(RowLayout::create()
                ->setAutoScale(0)
                ->setAxisAlignment(AxisAlignment::Start)
            );
            menu->addChild(name_container);

            auto dev = CCLabelBMFont::create(
                ModMetadata::formatDeveloperDisplayString(mod->getDevelopers()).data(),
                "goldFont.fnt"
            );
            dev->setLayoutOptions(AnchorLayoutOptions::create()
                ->setAnchor(Anchor::Left)
                ->setOffset({ m_bigView ? 44.f : 40.f, -6.f })
            );
            dev->setID("dev");
            dev->setScale(0.4f);
            dev->setAnchorPoint({ 0.f, 0.5f });
            menu->addChild(dev);

            auto view = CCMenuItemExt::createSpriteExtra(
                ButtonSprite::create(
                    "View", "bigFont.fnt",
                    (loaderMod->getSettingValue<bool>("enable-geode-theme"))
                    ? "geode.loader/GE_button_05.png" : "GJ_button_01.png"
                    , 0.70f
                ),
                [this, mod, mod_entry](auto) {
                    geode::openInfoPopup(mod);
                    ModPopup* modPopup = cocos::getChildOfType<ModPopup>(
                        this->getParent(), 0
                    );
                    if (modPopup) {
                        if (auto modLogo = modPopup->getChildByIDRecursive("mod-logo"))
                            ModLogoLoader::attach(mod, modLogo);
                        CCMenu* fk = cocos::findFirstChildRecursive<CCMenu>(
                            modPopup->m_mainLayer, [](CCNode* node) {
                                return nullptr != 
                                    node->getParent()->getParent()
                                    ->getChildByID("mod-id-label");
                            }
                        );
                        if (fk) {
                            fk->removeAllChildrenWithCleanup(0);
                            auto dwnloadbtnspr = ButtonSprite::create(
                                Loader::get()->isModLoaded(mod->getID()) ? 
                                "     Update     " : 
                                "    Download    ", 
                                "bigFont.fnt", 
                                Loader::get()->isModLoaded(mod->getID()) ? 
                                "geode.loader/GE_button_01.png" : 
                                "GJ_button_02.png",
                                0.60f
                            );
                            dwnloadbtnspr->setScale(0.5f);
                            auto download = CCMenuItemExt::createSpriteExtra(
                                dwnloadbtnspr,
                                [this, mod, mod_entry, dwnloadbtnspr](auto) {
                                    if (dwnloadbtnspr->m_label->getString() == std::string("Restart Game")) {
                                        game::restart();
                                    }
                                    auto req = web::WebRequest();
                                    auto listener = new EventListener<web::WebTask>;
                                    listener->bind(
                                        [this, mod, mod_entry, dwnloadbtnspr](web::WebTask::Event* e) {
                                            if (not typeinfo_cast<ModsLayer*>(this)) return e->cancel();
                                            if (web::WebProgress* prog = e->getProgress()) {
                                                dwnloadbtnspr->m_label->setString(
                                                    fmt::format(
                                                        "Downloading: {}%", 
                                                        (int)prog->downloadProgress().value_or(0.0)
                                                    ).data()
                                                );
                                            }
                                            if (web::WebResponse* res = e->getValue()) {
                                                std::string data = res->string().unwrapOr("no res");
                                                if (res->code() < 399) {
                                                    res->into(dirs::getModsDir() / (mod->getID() + ".geode"));
                                                    dwnloadbtnspr->m_label->setString(
                                                        fmt::format("Restart Game").data()
                                                    );
                                                    loaderMod->enable();
                                                }
                                                else {
                                                    auto asd = geode::createQuickPopup(
                                                        "Request exception",
                                                        data,
                                                        "Nah", nullptr, 420.f, nullptr, false
                                                    );
                                                    asd->show();
                                                };
                                            }
                                        }
                                    );
                                    listener->setFilter(req.send(
                                        "GET", mod_entry["versions"][0]["download_link"].as_string()
                                    ));
                                }
                            );
                            fk->addChild(download);
                            fk->updateLayout();
                        };
                    }
                }
            );
            view->setID("view");
            view->setLayoutOptions(AnchorLayoutOptions::create()
                ->setAnchor(Anchor::Right)
                ->setOffset({ -28.000f, 0.f })
            );
            view->setScale(0.5f);
            view->m_baseScale = view->getScale();
            menu->addChild(view);

            menu->updateLayout();
            customList->m_contentLayer->addChild(menu);
            //make content layer longer
            customList->m_contentLayer->setContentHeight(
                customList->m_contentLayer->getContentHeight() 
                + menu->getContentHeight()
                + (((ColumnLayout*)customList->m_contentLayer->getLayout())->getGap() / 1)
            );
        }
        //fix some shit goes when content smaller than scroll
        if (customList->m_contentLayer->getContentSize().height < customList->getContentSize().height) {
            customList->m_contentLayer->setContentSize({
                customList->m_contentLayer->getContentSize().width,
                customList->getContentSize().height
                });
        }
        //updataw
        customList->m_contentLayer->updateLayout();
        if (customListPoint == CCPointZero) customList->scrollToTop();
        else customList->m_contentLayer->setPosition(customListPoint);
        customList->scrollLayer(0.f);
    }
    void loadModsList() {
        //get nodes
        ModList* pModList = nullptr;
        CCMenuItemToggler* unverifiedModsToggle = nullptr;
        {
            if (pModList = typeinfo_cast<ModList*>(this->getChildByIDRecursive("ModList"))) void(); else
                return log::error("{}.ModList\" = {}", this, pModList);
            if (unverifiedModsToggle = typeinfo_cast<CCMenuItemToggler*>(this->getChildByIDRecursive("unverifiedModsToggle"_spr))) void(); else
                return log::error("{}.unverifiedModsToggle\" = {}", this, unverifiedModsToggle);
        };
        pModList->m_statusContainer->setVisible(true);
        //listener
        auto listener = new EventListener<web::WebTask>;
        listener->bind(
            [this, pModList, unverifiedModsToggle](web::WebTask::Event* e) {
                if (not typeinfo_cast<ModsLayer*>(this)) return e->cancel();
                if (not unverifiedModsToggle->isOn()) return e->cancel();
                if (web::WebResponse* res = e->getValue()) {
                    auto data = res->string().unwrapOr("no res");
                    auto jsonParseErr = std::string();
                    auto jsonParse = matjson::parse(data, jsonParseErr);
                    if ((res->code() < 399) and jsonParse.has_value()) {
                        //setup list
                        setupModsList(jsonParse.value());
                    }
                    else {
                        auto asd = geode::createQuickPopup(
                            "Request exception",
                            fmt::format(
                                "{}\n"
                                "code: {}, json parse: {}",
                                data,
                                res->code(), jsonParseErr.empty() ? "is ok" : jsonParseErr
                            ),
                            "Nah", nullptr, 420.f, nullptr, false
                        );
                        asd->show();
                    }
                    pModList->m_statusContainer->setVisible(false);
                }
            }
        );
        //tags
        auto tags = std::string();
        for (std::string tag : this->m_currentSource->getModTags()) {
            tags.append(tag + ",");
        }
        if (not tags.empty()) tags.pop_back();
        //page
        auto page = unverifiedModsToggle->getTag();
        page = page <= 1 ? 1 : page;
        unverifiedModsToggle->setTag(page);
        //req
        auto req = web::WebRequest();
        req.param("status", "pending");
        req.param("per_page", "10");
        req.param("sort", "recently_updated");
        req.param("page", std::to_string(page)); log::debug("{}", page);
        req.param("query", pModList->m_searchInput->getString());
        if (not tags.empty()) req.param("tags", tags);
        req.param("gd", utils::numToString<float>(GEODE_GD_VERSION, 3));
        req.param("platforms", GEODE_PLATFORM_SHORT_IDENTIFIER);
        req.header("Content-Type", "application/json");
        listener->setFilter(req.send("GET", "https://api.geode-sdk.org/v1/mods"));
    }
    void customSetup() {

        CCMenu* listActionsMenu = typeinfo_cast<CCMenu*>(this->getChildByIDRecursive("list-actions-menu"));
        if (not listActionsMenu) return log::error(
            "can't get \"list-actions-menu\" ({}) in {}",
            listActionsMenu, this
        );

        CCMenuItemToggler* unverifiedModsToggle = CCMenuItemExt::createToggler(
            ButtonSprite::create(CCSprite::create("hi.png"_spr), 32.5f, 0.f, 0, 0.85f, 0, (
                (loaderMod->getSettingValue<bool>("enable-geode-theme"))
                ? "geode.loader/GE_button_05.png" : "GJ_button_01.png"
                ), 0),
            ButtonSprite::create(CCSprite::create("hi.png"_spr), 32.5f, 0.f, 0, 0.85f, 0, "GJ_button_02.png", 1),
            [this](CCMenuItemToggler* btn) {
                //gototab0 installed tab
                m_tabs[1]->activate();//temp (to actually reopen tab0)
                m_tabs[0]->activate();
                //get nodes
                ModList* pModList = nullptr;
                {
                    if (pModList = typeinfo_cast<ModList*>(this->getChildByIDRecursive("ModList"))) void(); else
                        return log::error("{}.ModList\" = {}", this, pModList);
                };
                //setsmth
                pModList->m_list->setVisible(btn->isOn());
                this->m_goToPageBtn->setVisible(btn->isOn());
                typeinfo_cast<CCMenu*>(m_tabs[0]->getParent())->setTouchEnabled(btn->isOn());
                pModList->removeChildByID("customList"_spr);
                if (btn->isOn()) return;
                pModList->m_pagePrevBtn->setVisible(0);
                pModList->m_pageNextBtn->setVisible(0);
                loadModsList();
                //color out tab
                static CCScale9Sprite* tab0_bgDisabled = nullptr;
                static CCScale9Sprite* tab0_bgEnabled = nullptr;
                cocos::findFirstChildRecursive<CCScale9Sprite>(this->getChildByIDRecursive("installed-button"),
                    [this](CCScale9Sprite* node) {
                        if (not node->isVisible()) tab0_bgDisabled = node;
                        if (node->isVisible()) tab0_bgEnabled = node;
                        return false;
                    }
                );
                if (tab0_bgDisabled and tab0_bgEnabled) {
                    tab0_bgDisabled->setVisible(!tab0_bgDisabled->isVisible());
                    tab0_bgEnabled->setVisible(!tab0_bgEnabled->isVisible());
                }
            }
        );
        unverifiedModsToggle->setID("unverifiedModsToggle"_spr);
        unverifiedModsToggle->setTag(1);//HERE PAGE HUH

        listActionsMenu->addChild(unverifiedModsToggle, -10);
        listActionsMenu->updateLayout();

        //si
        cocos::findFirstChildRecursive<ModList>(this,
            [this, unverifiedModsToggle](ModList* pModList) {
                auto searchInputOldCallback = public_cast(pModList->m_searchInput, m_onInput);
                pModList->m_searchInput->setCallback(
                    [this, unverifiedModsToggle, searchInputOldCallback](auto asd) {
                        if (not unverifiedModsToggle->isOn())
                            return searchInputOldCallback(asd);
                        this->loadModsList();
                    }
                );
                if (auto item = pModList->m_pageNextBtn) {
                    auto orgSelectorContainer = CCMenuItem::create(
                        item->m_pListener, item->m_pfnSelector
                    );
                    orgSelectorContainer->setTag(item->getTag());
                    orgSelectorContainer->setID("orgSelectorContainer");
                    item->addChild(orgSelectorContainer);
                    CCMenuItemExt::assignCallback<CCMenuItem>(item,
                        [this, unverifiedModsToggle, pModList, orgSelectorContainer](CCMenuItem* sitem) {
                            if (not unverifiedModsToggle->isOn())
                                return orgSelectorContainer->activate();
                            sitem->setVisible(1);
                            auto page = unverifiedModsToggle->getTag();
                            page = page + 1;
                            unverifiedModsToggle->setTag(page);
                            this->loadModsList();
                        }
                    );
                }
                if (auto item = pModList->m_pagePrevBtn) {
                    auto orgSelectorContainer = CCMenuItem::create(
                        item->m_pListener, item->m_pfnSelector
                    );
                    orgSelectorContainer->setTag(item->getTag());
                    orgSelectorContainer->setID("orgSelectorContainer");
                    item->addChild(orgSelectorContainer);
                    CCMenuItemExt::assignCallback<CCMenuItem>(item,
                        [this, unverifiedModsToggle, pModList, orgSelectorContainer](CCMenuItem* sitem) {
                            if (not unverifiedModsToggle->isOn()) 
                                return orgSelectorContainer->activate();
                            sitem->setVisible(1);
                            auto page = unverifiedModsToggle->getTag();
                            page = page - 1;
                            unverifiedModsToggle->setTag(page);
                            this->loadModsList();
                        }
                    );
                }
                return false;
            }
        );
        cocos::findFirstChildRecursive<CCMenuItem>(this,
            [this](CCMenuItem* item) {
                if (item->getID() == "list-size-button") {
                    auto orgSelectorContainer = CCMenuItem::create(
                        item->m_pListener, item->m_pfnSelector
                    );
                    orgSelectorContainer->setID("orgSelectorContainer");
                    item->addChild(orgSelectorContainer);
                    CCMenuItemExt::assignCallback<CCMenuItem>(item,
                        [this, orgSelectorContainer](CCMenuItem* sitem) {
                            orgSelectorContainer->activate();
                            this->resetModsList();
                        }
                    );
                }
                if (item->getID() == "search-button") {
                    auto orgSelectorContainer = CCMenuItem::create(
                        item->m_pListener, item->m_pfnSelector
                    );
                    orgSelectorContainer->setID("orgSelectorContainer");
                    item->addChild(orgSelectorContainer);
                    CCMenuItemExt::assignCallback<CCMenuItem>(item,
                        [this, orgSelectorContainer](CCMenuItem* sitem) {
                            orgSelectorContainer->activate();
                            this->resetModsList();
                        }
                    );
                }
                return false;
            }
        );
    }
};

#include <Geode/modify/CCLayer.hpp>
class $modify(ModsLayerExt, CCLayer) {
    void tryCustomSetup(float) {
        if (auto q = typeinfo_cast<ModsLayer*>(this)) q->customSetup();
    }
	$override bool init() {
        this->scheduleOnce(schedule_selector(ModsLayerExt::tryCustomSetup), -1.f);
		return CCLayer::init();
	}
};