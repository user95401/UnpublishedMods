#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

static auto loaderMod = Loader::get()->getLoadedMod("geode.loader");

class ModPopup : public FLAlertLayer {};

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
    void setupMods(matjson::Value json) {
        //get nodes
        ModList* pModList = nullptr;
        {
            if (pModList = typeinfo_cast<ModList*>(this->getChildByIDRecursive("ModList"))) void(); else
                return log::error("{}.ModList\" = {}", this, pModList);
        };

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
        for (auto mod_entry : json["payload"]["data"].as_array()) {

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
                auto modmetaresult = ModMetadata::create(version);
                if (modmetaresult.has_value()) void();
                else return log::error("failed get mod meta: {}", modmetaresult.err());
                auto modmeta = modmetaresult.value();
                modmeta.setDetails(mod_entry.try_get<std::string>("about").value_or(""));
                modmeta.setChangelog(mod_entry.try_get<std::string>("changelog").value_or(""));
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

            auto logo = geode::createServerModLogo(mod->getID());
            logo->setLayoutOptions(AnchorLayoutOptions::create()
                ->setAnchor(Anchor::Left)
                ->setOffset({ 20.f, 0.f })
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
            ver->setScale(m_bigView ? 0.4 : 0.3f);
            ver->setColor({ 112, 235, 41 });

            auto name = CCLabelBMFont::create(
                mod->getName().data(), "bigFont.fnt"
            );
            name->setID("name");
            name->setScale(m_bigView ? 0.425 : 0.375f);

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
                                                        "{}% / 100%", 
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
        customList->scrollToTop();
    }
    void loadMods() {
        //get nodes
        ModList* pModList = nullptr;
        CCMenuItemToggler* unverifiedModsToggle = nullptr;
        {
            if (pModList = typeinfo_cast<ModList*>(this->getChildByIDRecursive("ModList"))) void(); else
                return log::error("{}.ModList\" = {}", this, pModList);
            if (unverifiedModsToggle = typeinfo_cast<CCMenuItemToggler*>(this->getChildByIDRecursive("unverifiedModsToggle"_spr))) void(); else
                return log::error("{}.unverifiedModsToggle\" = {}", this, unverifiedModsToggle);
        };
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
                        setupMods(jsonParse.value());
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
        //req
        auto req = web::WebRequest();
        req.param("status", "pending");
        req.param("per_page", "100");
        req.param("page", std::to_string(pModList->m_page));
        req.param("query", pModList->m_searchInput->getString());
        if (not tags.empty()) req.param("tags", tags);
        req.param("gd", utils::numToString<float>(GEODE_GD_VERSION, 3));
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
                //gotodownloadtab
                m_tabs[0]->activate();
                //get nodes
                ModList* pModList = nullptr;
                {
                    if (pModList = typeinfo_cast<ModList*>(this->getChildByIDRecursive("ModList"))) void(); else
                        return log::error("{}.ModList\" = {}", this, pModList);
                };
                //setsmth
                pModList->removeChildByID("customList"_spr);
                pModList->m_list->setVisible(btn->isOn());
                pModList->m_pagePrevBtn->setVisible(btn->isOn());
                pModList->m_pageNextBtn->setVisible(btn->isOn());
                pModList->m_statusContainer->setVisible(not btn->isOn());
                typeinfo_cast<CCMenu*>(m_tabs[2]->getParent())->setTouchEnabled(btn->isOn());
                loadMods();
            }
        );
        unverifiedModsToggle->setID("unverifiedModsToggle"_spr);

        listActionsMenu->addChild(unverifiedModsToggle, -10);
        listActionsMenu->updateLayout();

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