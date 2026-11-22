#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/EditLevelLayer.hpp>

using namespace geode::prelude;

// Fila 1: "In Editor"  -> m_workingTime
// Fila 2: "Editor (C)" -> m_workingTime2
// Si el tiempo es 0 el juego lo muestra como N/A.
class EditInfoPopup : public Popup {
protected:
    GJGameLevel* m_level = nullptr;
    TextInput* m_inputs[2][3] = {};

    static int totalSeconds(TextInput* h, TextInput* m, TextInput* s) {
        long long hh = numFromString<long long>(h->getString()).unwrapOr(0);
        long long mm = numFromString<long long>(m->getString()).unwrapOr(0);
        long long ss = numFromString<long long>(s->getString()).unwrapOr(0);
        long long total = hh * 3600 + mm * 60 + ss;
        if (total < 0) total = 0;
        if (total > 2000000000LL) total = 2000000000LL;
        return static_cast<int>(total);
    }

    void fillRow(int row, int total) {
        m_inputs[row][0]->setString(fmt::format("{}", total / 3600));
        m_inputs[row][1]->setString(fmt::format("{}", (total % 3600) / 60));
        m_inputs[row][2]->setString(fmt::format("{}", total % 60));
    }

    void addRow(int row, char const* label, float y, int total) {
        auto text = CCLabelBMFont::create(label, "goldFont.fnt");
        text->setScale(0.6f);
        m_mainLayer->addChildAtPosition(text, Anchor::Center, {0.f, y + 27.f});

        float xs[3] = {-95.f, -35.f, 25.f};
        char const* units[3] = {"h", "m", "s"};
        for (int i = 0; i < 3; i++) {
            auto input = TextInput::create(50.f, units[i]);
            input->setFilter("0123456789");
            input->setMaxCharCount(i == 0 ? 6 : 2);
            m_mainLayer->addChildAtPosition(input, Anchor::Center, {xs[i], y});
            m_inputs[row][i] = input;

            auto u = CCLabelBMFont::create(units[i], "bigFont.fnt");
            u->setScale(0.5f);
            m_mainLayer->addChildAtPosition(u, Anchor::Center, {xs[i] + 36.f, y});
        }
        this->fillRow(row, total);

        auto naSpr = ButtonSprite::create("N/A", 40, true, "bigFont.fnt", "GJ_button_06.png", 25.f, 0.6f);
        auto naBtn = CCMenuItemSpriteExtra::create(
            naSpr, this, row == 0 ? menu_selector(EditInfoPopup::onNA0) : menu_selector(EditInfoPopup::onNA1)
        );
        m_buttonMenu->addChildAtPosition(naBtn, Anchor::Center, {125.f, y});
    }

    void onNA0(CCObject*) { this->fillRow(0, 0); }
    void onNA1(CCObject*) { this->fillRow(1, 0); }

    void onSave(CCObject*) {
        m_level->m_workingTime = totalSeconds(m_inputs[0][0], m_inputs[0][1], m_inputs[0][2]);
        m_level->m_workingTime2 = totalSeconds(m_inputs[1][0], m_inputs[1][1], m_inputs[1][2]);

        // Los niveles locales (del editor) se guardan en disco.
        // El servidor recibe el tiempo cuando subes / actualizas el nivel.
        bool isLocal = m_level->m_levelType == GJLevelType::Editor;
        if (isLocal) {
            GameLevelManager::get()->saveLevel(m_level);
        }

        this->onClose(nullptr);

        if (isLocal) {
            FLAlertLayer::create(
                "Edit Info",
                "Tiempo guardado en tu nivel.\nPara que quede en el <cy>servidor</c>, sube o actualiza el nivel (boton de compartir/Update).",
                "OK"
            )->show();
        } else {
            FLAlertLayer::create(
                "Edit Info",
                "Cambiado solo en tu pantalla.\nPara cambiarlo en el <cy>servidor</c>, abre tu copia local del nivel, editalo ahi y actualiza el nivel.",
                "OK"
            )->show();
        }
    }

    bool init(GJGameLevel* level) {
        if (!Popup::init(340.f, 230.f)) return false;
        m_level = level;
        this->setTitle("Edit Info");

        this->addRow(0, "In Editor", 35.f, level->m_workingTime);
        this->addRow(1, "Editor (C)", -30.f, level->m_workingTime2);

        auto saveSpr = ButtonSprite::create("Save");
        auto saveBtn = CCMenuItemSpriteExtra::create(saveSpr, this, menu_selector(EditInfoPopup::onSave));
        m_buttonMenu->addChildAtPosition(saveBtn, Anchor::Center, {0.f, -82.f});

        return true;
    }

public:
    static EditInfoPopup* create(GJGameLevel* level) {
        auto ret = new EditInfoPopup();
        if (ret->init(level)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

static CCMenuItemSpriteExtra* makeEditButton(CCObject* target, SEL_MenuHandler handler) {
    auto spr = ButtonSprite::create("Edit", 40, true, "bigFont.fnt", "GJ_button_04.png", 30.f, 0.6f);
    auto btn = CCMenuItemSpriteExtra::create(spr, target, handler);
    btn->setID("edit-info-button"_spr);
    return btn;
}

static void addToMenuOrFallback(CCNode* layer, CCNode* menu, CCMenuItemSpriteExtra* btn) {
    if (menu) {
        menu->addChild(btn);
        menu->updateLayout();
    } else {
        auto fallback = CCMenu::create();
        auto win = CCDirector::get()->getWinSize();
        fallback->setPosition({win.width - 40.f, 120.f});
        fallback->addChild(btn);
        layer->addChild(fallback);
    }
}

// Página del nivel (online / GDPS)
class $modify(EditInfoLevelInfo, LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) return false;
        auto btn = makeEditButton(this, menu_selector(EditInfoLevelInfo::onEditInfo));
        addToMenuOrFallback(this, this->getChildByID("left-side-menu"), btn);
        return true;
    }

    void onEditInfo(CCObject*) {
        if (auto popup = EditInfoPopup::create(m_level)) popup->show();
    }
};

// Página del nivel local (Mis niveles)
class $modify(EditInfoEditLevel, EditLevelLayer) {
    bool init(GJGameLevel* level) {
        if (!EditLevelLayer::init(level)) return false;
        auto btn = makeEditButton(this, menu_selector(EditInfoEditLevel::onEditInfo));
        addToMenuOrFallback(this, this->getChildByID("level-actions-menu"), btn);
        return true;
    }

    void onEditInfo(CCObject*) {
        if (auto popup = EditInfoPopup::create(m_level)) popup->show();
    }
};
