#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/utils/web.hpp>

#include <_a_autoupdate.hpp>

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

auto enabled = false;

web::WebTask WebRequest_send(web::WebRequest* self, std::string_view method, std::string_view givenUrl) {
    if (enabled and string::contains(givenUrl.data(), "api.geode-sdk.org/v1/mods")) {
        self->param("status", "pending");
    }
    log::debug("{}(std::string_view {}, std::string_view {})", __func__, method, givenUrl);
    return self->send(method, givenUrl);
};

Hook* MY_WebRequest_send_HOOK = nullptr;
std::vector<Hook*> WebRequest_send_HOOKS_LIST;
void INIT_WebRequest_send_HOOKS_LIST() {
    if (WebRequest_send_HOOKS_LIST.empty()) {
        for (auto mod : Loader::get()->getAllMods()) for (auto hook : mod->getHooks()) {
            if (string::contains(hook->getDisplayName().data(), "WebRequest::send")) {
                WebRequest_send_HOOKS_LIST.push_back(hook);
            }
        };
    }
}
void TOGGLE_MAIN() {
    //toggle
    if (enabled) {
        enabled = 0;
        //enable not my ones
        INIT_WebRequest_send_HOOKS_LIST();
        for (auto hook : WebRequest_send_HOOKS_LIST) {
            log::debug("result: {}", hook->enable().error_or("is ok."));
            log::debug("hook: {}", hook->getRuntimeInfo().dump());
        }
        //disable my
        log::debug("result: {}", MY_WebRequest_send_HOOK->disable().error_or("is ok."));
        log::debug("hook: {}", MY_WebRequest_send_HOOK->getRuntimeInfo().dump());
    }
    else {
        enabled = 1;
        //disable not my ones
        INIT_WebRequest_send_HOOKS_LIST();
        for (auto hook : WebRequest_send_HOOKS_LIST) {
            log::debug("result: {}", hook->disable().error_or("is ok."));
            log::debug("hook: {}", hook->getRuntimeInfo().dump());
        };
        //enable my
        if (MY_WebRequest_send_HOOK) log::debug("result: {}", MY_WebRequest_send_HOOK->enable().error_or("is ok."));
        else MY_WebRequest_send_HOOK = Mod::get()->hook(
            reinterpret_cast<void*>(getNonVirtual(&web::WebRequest::send)),
            &WebRequest_send, "web::WebRequest::send"_spr, tulip::hook::TulipConvention::Thiscall
        ).value();
        log::debug("hook: {}", MY_WebRequest_send_HOOK->getRuntimeInfo().dump());
    }
}

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

#include <Geode/modify/CCMenuItem.hpp>
class $modify(ModListButtons, CCMenuItem) {
    $override void activate() {
        CCMenuItem::activate();
        if (!this->m_pListener) return;
        auto listener = typeinfo_cast<CCNode*>(this->m_pListener);
        if (this->getID() == "filters-button" and listener->getID() == "ModList") {
            findFirstChildRecursive<FLAlertLayer>(CCScene::get(),
                [](FLAlertLayer* __this) {

                    //is "Search Filters"
                    if (!findFirstChildRecursive<CCLabelBMFont>(
                        __this, [](CCLabelBMFont* lbl) {
                            return lbl->getString() == std::string("Search Filters");
                        })) return false;

                    auto toggle = CCMenuItemExt::createTogglerWithStandardSprites(0.6,
                        [](auto) {
                            TOGGLE_MAIN();
                        }
                    );
                    toggle->toggle(enabled);
                    toggle->setPosition(20, 20);

                    __this->m_buttonMenu->addChild(toggle);

                    auto Label = CCLabelBMFont::create("Unverified", "bigFont.fnt");
                    Label->setScale(0.40f);
                    Label->setAnchorPoint({ 0.f, 0.5f });
                    Label->setPosition(32, 20);
                    __this->m_buttonMenu->addChild(Label);

                    return true; 
                }
            );
        }
    }
};

#if 0
#endif