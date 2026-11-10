#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <chrono>
#include <random>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>
#include <array>

using namespace geode::prelude;

// pelimuodot: 0 cube, 1 ship, 2 ball, 3 ufo, 4 wave, 5 robot, 6 spider, 7 swing
static constexpr int MODE_COUNT = 8;

// ============================================================
//  ASETUKSET
// ============================================================
struct BuildSettings {
    int objects = 1000;
    int seconds = 20;
    int difficulty = 3;   // 1 = Easy ... 5 = Extreme Demon
    bool modes[MODE_COUNT] = {true, true, true, true, true, true, true, true};
    bool moveTriggers = true;
};

// ============================================================
//  GENERAATTORI
// ============================================================
struct Gen {
    LevelEditorLayer* lel = nullptr;
    std::mt19937 rng{std::random_device{}()};
    BuildSettings s;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    float x = 900.f;
    int made = 0;
    float mul = 1.f;
    int nextGroup = 1;
    bool started = false;
    bool finished = false;
    std::vector<int> modes;

    static constexpr int ROWS = 10;

    int rnd(int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng); }
    bool chance(float p) { return std::uniform_real_distribution<float>(0.f, 1.f)(rng) < p; }
    float row(int i) { return 15.f + 30.f * i; }
    bool extreme() const { return s.difficulty >= 5; }

    bool done() {
        float el = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
        return made >= s.objects || el >= static_cast<float>(s.seconds);
    }
    GameObject* put(int id, float px, float py) {
        made++;
        return lel->createObject(id, {px, py}, false);
    }
    void rest(float extra = 0.f) {
        float units = std::max(extreme() ? 3.5f : 4.f, (9.f - s.difficulty) * mul + extra);
        x += 30.f * std::round(units);
    }

    // ----- Portaalit (mini-koko vain extremessa) -----
    void enter(int portalId, bool mini = false) {
        put(portalId, x, row(1));
        if (mini) put(101, x + 60.f, row(1));          // mini-portaali
        x += 30.f * 6;
    }
    void exitToCube(bool mini = false) {
        const int rows[] = {1, 4, 7, 9};
        for (int r : rows) {
            put(12, x, row(r));
            if (mini) put(99, x + 30.f, row(r));       // normaalikoko takaisin
        }
        x += 30.f * 10;
    }

    // ----- Move trigger: portti aukeaa ennen kuin pelaaja ehtii luo -----
    void gate() {
        int group = nextGroup++;
        float lead = 30.f * std::round((extreme() ? 7.f : 10.f) * mul);
        auto trig = static_cast<EffectGameObject*>(put(901, x, row(11)));
        if (trig) {
            trig->m_moveOffset = ccp(0.f, 300.f);
            trig->m_duration = extreme() ? 0.3f : 0.5f;
            trig->m_targetGroupID = group;
        }
        x += lead;
        for (int w = 0; w < 2; w++)
            for (int r = 0; r < 4; r++) {
                auto b = put(1, x + 30.f * w, row(r));
                if (b) b->addToGroup(group);
            }
        x += 60.f;
    }

    // ----- Maakuviot (cube, robot) -----
    void spikes(int n) {
        for (int i = 0; i < n; i++) put(8, x + 30.f * i, row(0));
        x += 30.f * n;
    }
    void spikeRun() {                                   // rytmisarja yksittaisia piikkeja
        int n = rnd(3, 6);
        int sp = std::max(5, static_cast<int>(std::round(5.5f * mul)));
        for (int i = 0; i < n && !done(); i++) {
            put(8, x, row(0));
            x += 30.f * sp;
        }
    }
    void stairs() {
        int steps = rnd(2, 3);
        for (int h = 1; h <= steps; h++) {
            for (int w = 0; w < 3; w++)
                for (int r = 0; r < h; r++) put(1, x + 30.f * w, row(r));
            x += 90.f;
        }
        for (int h = steps - 1; h >= 1; h--) {
            for (int w = 0; w < 3; w++)
                for (int r = 0; r < h; r++) put(1, x + 30.f * w, row(r));
            x += 90.f;
        }
    }
    void padOverSpikes() {
        int n = rnd(3, extreme() ? 6 : 5);
        put(35, x, row(0));
        x += 60.f;
        spikes(n);
    }
    void orbOverSpikes() {
        int n = rnd(4, extreme() ? 7 : 6);
        for (int i = 0; i < n; i++) put(8, x + 30.f * i, row(0));
        put(36, x + 30.f * (n / 2), row(3));
        x += 30.f * n;
    }

    void cubePattern() {
        int d = s.difficulty;
        float gateP = 0.15f;
        if (s.moveTriggers && chance(gateP)) {
            int count = extreme() ? rnd(2, 3) : 1;      // extremessa portteja ketjussa
            for (int i = 0; i < count; i++) gate();
            return;
        }
        float pPad = 0.10f;
        float pOrb = 0.08f;
        if (d >= 2 && chance(pPad)) { padOverSpikes(); return; }
        if (d >= 3 && chance(pOrb)) { orbOverSpikes(); return; }
        if (d >= 4 && chance(0.2f)) { spikeRun(); return; }
        int pick = rnd(0, 99);
        if (pick < 35)                          spikes(1);
        else if (pick < 60)                     spikes(2);
        else if (pick < 75 && d >= 3)           spikes(3);
        else if (pick < 85 && extreme())        spikes(4);
        else                                    stairs();
    }
    void robotPattern() {
        int pick = rnd(0, 99);
        if (pick < 40)      spikes(1);
        else if (pick < 70) spikes(2);
        else if (pick < 90) stairs();
        else if (s.difficulty >= 2) padOverSpikes();
        else spikes(1);
    }
    void jumpSection(bool robot) {
        int patterns = rnd(8, 14) + s.difficulty * 2;
        for (int i = 0; i < patterns && !done(); i++) {
            if (robot) robotPattern(); else cubePattern();
            rest();
        }
    }

    // ----- Kaytava (ship / ufo / wave / swing) -----
    void corridor(int gap, int maxStep, int spacing, int columns, bool smooth,
                  int runMin = 3, int runMax = 6) {
        int center = gap / 2 + 1;
        int lo = gap / 2 + 1, hi = ROWS - (gap - gap / 2) - 1;
        int dir = 1, runLeft = rnd(runMin, runMax);
        x += 30.f * 6;
        for (int c = 0; c < columns && !done(); c++) {
            if (smooth) {
                center += dir;
                if (--runLeft <= 0 || center <= lo || center >= hi) {
                    dir = -dir;
                    runLeft = rnd(runMin, runMax);
                }
            } else {
                center += rnd(-maxStep, maxStep);
            }
            center = std::clamp(center, lo, hi);
            int floorH = center - gap / 2;
            int ceilH = ROWS - (center + (gap - gap / 2));
            for (int r = 0; r < floorH; r++) put(1, x, row(r));
            for (int r = 0; r < ceilH; r++) put(1, x, row(ROWS - 1 - r));
            x += 30.f * spacing;
        }
        x += 30.f * 2;
    }
    void shipSection(bool mini) {
        int d = s.difficulty;
        int gap = std::max(4, 8 - d) - (mini ? 1 : 0);
        int step = extreme() ? (mini ? 2 : 3) : 2;
        int spacing = extreme() ? std::max(4, static_cast<int>(std::round(5.f * mul)))
                                : std::max(5, static_cast<int>(std::round(6.f * mul)));
        corridor(std::max(3, gap), step, spacing, rnd(10, 18) + d * 2, false);
    }
    void ufoSection(bool mini) {
        int d = s.difficulty;
        int gap = std::max(5, 9 - d) - (mini ? 1 : 0);
        int spacing = std::max(extreme() ? 5 : 6, static_cast<int>(std::round((extreme() ? 6.f : 7.f) * mul)));
        corridor(std::max(4, gap), 1, spacing, rnd(8, 14) + d * 2, false);
    }
    void swingSection(bool mini) {
        int d = s.difficulty;
        int gap = std::max(5, 9 - d) - (mini ? 1 : 0);
        int spacing = std::max(extreme() ? 5 : 6, static_cast<int>(std::round((extreme() ? 6.f : 7.f) * mul)));
        corridor(std::max(4, gap), extreme() ? 2 : 1, spacing, rnd(9, 15) + d * 2, false);
    }
    void waveSection(bool mini) {
        int d = s.difficulty;
        int gap = std::max(3, 7 - d) - (mini ? 1 : 0);
        int runMin = std::max(2, 7 - d);
        corridor(std::max(2, gap), 1, 1, rnd(50, 90) + d * 10, true, runMin, runMin + 2);
    }

    // ----- Ball / Spider -----
    void gravitySection() {
        int d = s.difficulty;
        int obstacles = rnd(8, 14) + d * 2;
        int spacingCols = std::max(5, static_cast<int>(std::round((10.f - d) * mul)));
        int totalCols = obstacles * spacingCols;
        for (int c = 0; c < totalCols && !done(); c++)
            put(1, x + 30.f * c, row(ROWS - 1));
        float doubleP = extreme() ? 0.5f : (d >= 4 ? 0.3f : 0.f);
        for (int i = 0; i < obstacles && !done(); i++) {
            x += 30.f * spacingCols;
            int n = chance(doubleP) ? 2 : 1;
            bool ceiling = chance(0.5f);
            for (int k = 0; k < n; k++) {
                float px = x + 30.f * k;
                if (ceiling) {
                    auto sp = put(8, px, row(ROWS - 2));
                    if (sp) sp->setRotation(180.f);
                } else {
                    put(8, px, row(0));
                }
            }
        }
        x += 30.f * 4;
    }

    void maybeSpeed() {
        if (s.difficulty < 2) return;
        if (!extreme() && !chance(0.5f)) return;        // extremessa nopeus vaihtuu aina
        static const int ids[] = {200, 201, 202, 203, 1334};
        static const float muls[] = {0.8f, 1.f, 1.24f, 1.5f, 1.85f};
        int hi = std::min(4, s.difficulty - 1);
        int lo = std::min(extreme() ? 2 : 1, hi);       // extreme: vahintaan Fast
        int idx = rnd(lo, hi);
        put(ids[idx], x, row(1));
        mul = muls[idx];
        x += 30.f * 6;
    }

    int pickMode() { return modes[rnd(0, static_cast<int>(modes.size()) - 1)]; }

    void prepare() {
        modes.clear();
        for (int i = 0; i < MODE_COUNT; i++) if (s.modes[i]) modes.push_back(i);
        if (modes.empty()) modes.push_back(0);
        start = std::chrono::steady_clock::now();
    }

    // Yksi osio per kutsu: kutsuja jakaa tyon kehyksille, ettei puhelin jaady
    void step() {
        if (finished) return;
        if (done()) { finished = true; return; }
        if (!started) {
            put(12, 300.f, row(1));
            started = true;
        }
        int m = pickMode();
        bool mini = extreme() && (m == 1 || m == 3 || m == 4 || m == 7) && chance(0.4f);
        maybeSpeed();
        switch (m) {
            case 0: jumpSection(false); break;
            case 1: enter(13, mini);   shipSection(mini);  exitToCube(mini); break;
            case 2: enter(47);         gravitySection();   exitToCube();     break;
            case 3: enter(111, mini);  ufoSection(mini);   exitToCube(mini); break;
            case 4: enter(660, mini);  waveSection(mini);  exitToCube(mini); break;
            case 5: enter(745);        jumpSection(true);  exitToCube();     break;
            case 6: enter(1331);       gravitySection();   exitToCube();     break;
            case 7: enter(1933, mini); swingSection(mini); exitToCube(mini); break;
        }
        x += 30.f * 6;
        if (done()) finished = true;
    }
};

// Ajaa generaattoria kehys kerrallaan (n. 8 ms/kehys), jotta peli ei jaady puhelimella
class GenRunner : public CCNode {
protected:
    Gen m_gen;
    inline static bool s_active = false;

    bool init(LevelEditorLayer* lel, BuildSettings s) {
        if (!CCNode::init()) return false;
        m_gen.lel = lel;
        m_gen.s = s;
        m_gen.prepare();
        s_active = true;
        this->scheduleUpdate();
        return true;
    }

    void update(float) override {
        auto t0 = std::chrono::steady_clock::now();
        while (!m_gen.finished) {
            m_gen.step();
            float ms = std::chrono::duration<float, std::milli>(
                std::chrono::steady_clock::now() - t0).count();
            if (ms > 8.f) break;
        }
        if (m_gen.finished) {
            s_active = false;
            FLAlertLayer::create("Auto Builder",
                fmt::format("Built {} objects.", m_gen.made).c_str(), "OK")->show();
            this->unscheduleUpdate();
            this->removeFromParentAndCleanup(true);
        }
    }

public:
    static bool active() { return s_active; }
    static GenRunner* create(LevelEditorLayer* lel, BuildSettings s) {
        auto ret = new GenRunner();
        if (ret->init(lel, s)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ============================================================
//  VALIKKO (400 x 280: mahtuu puhelimen naytolle)
// ============================================================
class AutoBuildPopup : public geode::Popup {
protected:
    LevelEditorLayer* m_lel = nullptr;
    Slider* m_objSlider = nullptr;
    Slider* m_timeSlider = nullptr;
    Slider* m_diffSlider = nullptr;
    CCLabelBMFont* m_objLabel = nullptr;
    CCLabelBMFont* m_timeLabel = nullptr;
    CCLabelBMFont* m_diffLabel = nullptr;
    CCMenuItemToggler* m_modeToggles[MODE_COUNT] = {};
    CCMenuItemToggler* m_trigToggle = nullptr;

    BuildSettings current() {
        BuildSettings s;
        s.objects = 100 + static_cast<int>(std::round(m_objSlider->getValue() * 98)) * 50;
        s.seconds = 5 + static_cast<int>(std::round(m_timeSlider->getValue() * 115));
        s.difficulty = 1 + static_cast<int>(std::round(m_diffSlider->getValue() * 4));
        for (int i = 0; i < MODE_COUNT; i++) s.modes[i] = m_modeToggles[i]->isToggled();
        s.moveTriggers = m_trigToggle->isToggled();
        return s;
    }

    void refresh() {
        static const char* names[] = {"", "Easy", "Normal", "Hard", "Insane", "Extreme Demon"};
        auto s = current();
        m_objLabel->setString(fmt::format("Objects: {}", s.objects).c_str());
        m_timeLabel->setString(fmt::format("Time: {} s", s.seconds).c_str());
        m_diffLabel->setString(fmt::format("Difficulty: {}", names[s.difficulty]).c_str());
    }

    void onSlider(CCObject*) { refresh(); }
    void onToggle(CCObject*) {}

    void onBuild(CCObject*) {
        if (GenRunner::active()) return;
        auto s = current();
        if (auto runner = GenRunner::create(m_lel, s)) m_lel->addChild(runner);
        Notification::create("Building level...", NotificationIcon::Loading, 1.f)->show();
        this->onClose(nullptr);
    }

    CCLabelBMFont* addRow(float y, Slider*& slider, float initial) {
        auto label = CCLabelBMFont::create("", "bigFont.fnt");
        label->setScale(0.4f);
        label->setPosition({105.f, y + 16.f});
        m_mainLayer->addChild(label);

        slider = Slider::create(this, menu_selector(AutoBuildPopup::onSlider), 0.55f);
        slider->setPosition({105.f, y});
        slider->setValue(initial);
        m_mainLayer->addChild(slider);
        return label;
    }

    CCMenuItemToggler* addToggle(const char* name, float cx, float y) {
        auto t = CCMenuItemToggler::createWithStandardSprites(
            this, menu_selector(AutoBuildPopup::onToggle), 0.6f);
        t->setPosition({cx - 24.f, y});
        t->toggle(true);
        m_buttonMenu->addChild(t);

        auto l = CCLabelBMFont::create(name, "bigFont.fnt");
        l->setScale(0.3f);
        l->setAnchorPoint({0.f, 0.5f});
        l->setPosition({cx - 10.f, y});
        m_mainLayer->addChild(l);
        return t;
    }

    bool init(LevelEditorLayer* lel) {
        if (!Popup::init(400.f, 280.f)) return false;
        m_lel = lel;
        this->setTitle("Auto Builder");
        float h = m_size.height;

        m_objLabel  = addRow(h - 75.f,  m_objSlider,  0.2f);
        m_timeLabel = addRow(h - 125.f, m_timeSlider, 0.15f);
        m_diffLabel = addRow(h - 175.f, m_diffSlider, 0.5f);

        auto head = CCLabelBMFont::create("Gamemodes", "bigFont.fnt");
        head->setScale(0.4f);
        head->setPosition({290.f, h - 45.f});
        m_mainLayer->addChild(head);

        const char* names[MODE_COUNT] = {"Cube", "Ship", "Ball", "UFO", "Wave", "Robot", "Spider", "Swing"};
        for (int i = 0; i < MODE_COUNT; i++) {
            float cx = (i % 2 == 0) ? 235.f : 330.f;
            float y = h - 68.f - 29.f * (i / 2);
            m_modeToggles[i] = addToggle(names[i], cx, y);
        }
        m_trigToggle  = addToggle("Triggers", 235.f, h - 68.f - 29.f * 4);

        auto build = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Build"), this, menu_selector(AutoBuildPopup::onBuild));
        build->setPosition({105.f, 40.f});
        m_buttonMenu->addChild(build);

        refresh();
        return true;
    }

public:
    static AutoBuildPopup* create(LevelEditorLayer* lel) {
        auto ret = new AutoBuildPopup();
        if (ret->init(lel)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ============================================================
//  REMIX: kopioi kohtia OMISTA leveleistasi ja muokkaa niita hieman
// ============================================================
struct SrcObj {
    std::string seg;   // alkuperainen objektirivi
    float x = 0, y = 0;
    int id = 0;
};

struct Source {
    std::vector<SrcObj> objs;
    std::vector<float> cands;                    // sallitut aloituskohdat
    std::vector<std::pair<float, int>> speeds;   // (x, nopeusportaalin id)
};

static std::vector<std::string> splitCommas(const std::string& s) {
    std::vector<std::string> out;
    size_t pos = 0;
    while (true) {
        size_t c = s.find(',', pos);
        if (c == std::string::npos) { out.push_back(s.substr(pos)); break; }
        out.push_back(s.substr(pos, c - pos));
        pos = c + 1;
    }
    return out;
}

static std::string withPos(const std::string& seg, float nx, float ny) {
    auto t = splitCommas(seg);
    for (size_t k = 0; k + 1 < t.size(); k += 2) {
        int key = std::atoi(t[k].c_str());
        if (key == 2) t[k + 1] = fmt::format("{}", nx);
        else if (key == 3) t[k + 1] = fmt::format("{}", ny);
    }
    std::string out;
    for (size_t k = 0; k < t.size(); k++) {
        if (k) out += ',';
        out += t[k];
    }
    return out;
}

static bool isTriggerId(int id) {
    switch (id) {
        case 901: case 899: case 1007: case 1006: case 1268: case 1346: case 1347:
        case 1049: case 1616: case 1611: case 1912: case 2067: case 1595: case 1815:
        case 29: case 30: case 104: case 105: case 221: case 717: case 744:
        case 31:                                  // aloituspiste
            return true;
        default: return false;
    }
}
static bool isModePortal(int id) {
    return id == 12 || id == 13 || id == 47 || id == 111 || id == 660 ||
           id == 745 || id == 1331 || id == 1933;
}
static bool isSpeedPortal(int id) {
    return id == 200 || id == 201 || id == 202 || id == 203 || id == 1334;
}

// Jasentaa levelin: ohittaa triggerit ja ryhmitetyt objektit, koska niiden
// ryhma-/varikanavat voisivat sotkea toisten osioiden kanssa.
static bool parseSource(const std::string& data, Source& out) {
    size_t headerEnd = data.find(';');
    if (headerEnd == std::string::npos) return false;

    bool startsCube = true;
    {
        auto h = data.substr(0, headerEnd);
        size_t k = h.find("kA2,");
        if (k != std::string::npos && k + 4 < h.size()) startsCube = (h[k + 4] == '0');
    }

    float minX = 1e9f;
    size_t i = headerEnd;
    while (i != std::string::npos && i + 1 < data.size()) {
        size_t st = i + 1;
        size_t en = data.find(';', st);
        if (en == std::string::npos) en = data.size();
        if (en > st) {
            std::string seg = data.substr(st, en - st);
            auto t = splitCommas(seg);
            int id = 0;
            float x = 0, y = 0;
            bool hasX = false, skip = false;
            for (size_t k = 0; k + 1 < t.size(); k += 2) {
                int key = std::atoi(t[k].c_str());
                if (key == 1) id = std::atoi(t[k + 1].c_str());
                else if (key == 2) { x = static_cast<float>(std::atof(t[k + 1].c_str())); hasX = true; }
                else if (key == 3) y = static_cast<float>(std::atof(t[k + 1].c_str()));
                else if (key == 57 || key == 51) skip = true;
                else if (key == 36 && t[k + 1] == "1") skip = true;
            }
            if (hasX && !skip && !isTriggerId(id)) {
                out.objs.push_back({seg, x, y, id});
                minX = std::min(minX, x);
                if (isSpeedPortal(id)) out.speeds.push_back({x, id});
                if (isModePortal(id) && y < 100.f) out.cands.push_back(x - 60.f);
            }
        }
        i = en;
    }
    if (out.objs.size() < 30) return false;
    if (startsCube) out.cands.push_back(minX - 30.f);
    std::sort(out.speeds.begin(), out.speeds.end());
    return !out.cands.empty();
}

// Levittaa olemassa olevia tyhjia kohtia (tai kaventaa isoja) -> pieni muutos rytmiin,
// geometria ei muutu.
static void adjustGaps(std::vector<SrcObj>& v, int edits, std::mt19937& rng) {
    for (int e = 0; e < edits; e++) {
        std::vector<float> xs;
        xs.reserve(v.size());
        for (auto& o : v) xs.push_back(o.x);
        std::sort(xs.begin(), xs.end());
        xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
        std::vector<size_t> gaps;
        for (size_t i = 0; i + 1 < xs.size(); i++)
            if (xs[i + 1] - xs[i] >= 150.f) gaps.push_back(i);
        if (gaps.empty()) return;
        size_t g = gaps[rng() % gaps.size()];
        float gapLen = xs[g + 1] - xs[g];
        float delta = (gapLen >= 240.f && rng() % 2) ? -30.f : 30.f * static_cast<float>(1 + rng() % 2);
        float threshold = xs[g + 1];
        for (auto& o : v)
            if (o.x >= threshold) o.x += delta;
    }
}

struct RemixSettings {
    std::vector<GJGameLevel*> levels;
    int sections = 6;
    int variation = 40;     // 0-100
};

class RemixRunner : public CCNode {
protected:
    LevelEditorLayer* m_lel = nullptr;
    std::mt19937 m_rng{std::random_device{}()};
    std::vector<Source> m_src;
    int m_left = 0;
    int m_done = 0;
    int m_made = 0;
    int m_variation = 40;
    float m_x = 300.f;
    bool m_first = true;
    inline static bool s_active = false;

    int rnd(int a, int b) { return std::uniform_int_distribution<int>(a, b)(m_rng); }

    float existingEnd() {
        float maxX = 0.f;
        bool any = false;
        if (auto arr = m_lel->m_objects) {
            CCObject* o = nullptr;
            CCARRAY_FOREACH(arr, o) {
                if (auto go = typeinfo_cast<GameObject*>(o)) {
                    maxX = std::max(maxX, go->getPositionX());
                    any = true;
                }
            }
        }
        return any ? maxX : -1.f;
    }

    void put(std::string& out, int id, float x, float y) {
        out += fmt::format("1,{},2,{},3,{};", id, x, y);
        m_made++;
    }

    // Palauttaa pelaajan cubeen, normaalikokoon ja -painovoimaan sekä normaalinopeuteen
    void transition(std::string& out) {
        const int rows[] = {1, 4, 7, 9};
        const int ids[] = {12, 99, 10, 287, 201};
        for (int id : ids)
            for (int r : rows) put(out, id, m_x, 15.f + 30.f * r);
        m_x += 30.f * 10;                       // tilaa pudota maahan
    }

    bool buildSection() {
        for (int attempt = 0; attempt < 10; attempt++) {
            auto& src = m_src[rnd(0, static_cast<int>(m_src.size()) - 1)];
            float x0 = src.cands[rnd(0, static_cast<int>(src.cands.size()) - 1)];
            float width = static_cast<float>(rnd(900, 1800 + m_variation * 10));

            std::vector<SrcObj> win;
            for (auto& o : src.objs)
                if (o.x >= x0 && o.x < x0 + width) win.push_back(o);
            if (win.size() < 15) continue;

            for (auto& o : win) o.x -= x0;
            adjustGaps(win, m_variation / 20, m_rng);

            std::string out;
            // valiaika edellisen osion perassa: 2..(4 + variaatio) blokkia
            if (!m_first) m_x += 30.f * rnd(2, 4 + m_variation / 20);
            transition(out);

            // nopeus samaksi kuin alkuperaisessa kohdassa
            int speedId = 201;
            for (auto& sp : src.speeds)
                if (sp.first <= x0 + 60.f) speedId = sp.second;
            put(out, speedId, m_x, 45.f);
            m_x += 30.f * 2;

            float maxRel = 0.f;
            for (auto& o : win) {
                out += withPos(o.seg, m_x + o.x, o.y);
                out += ';';
                m_made++;
                maxRel = std::max(maxRel, o.x);
            }
            m_x += maxRel + 60.f;

            m_lel->createObjectsFromString(out, true, true);
            m_first = false;
            return true;
        }
        return false;
    }

    bool init(LevelEditorLayer* lel, RemixSettings const& rs) {
        if (!CCNode::init()) return false;
        m_lel = lel;
        m_left = rs.sections;
        m_variation = rs.variation;

        for (auto lvl : rs.levels) {
            std::string packed = lvl->m_levelString;
            std::string data = ZipUtils::decompressString(packed, false, 11);
            if (data.empty()) data = packed;     // jos taso on jo purettu
            Source src;
            if (parseSource(data, src)) m_src.push_back(std::move(src));
        }
        if (m_src.empty()) return false;

        float end = existingEnd();
        if (end >= 0.f) { m_x = end + 300.f; m_first = false; }  // nykyisen levelin perään
        s_active = true;
        this->scheduleUpdate();
        return true;
    }

    void update(float) override {
        if (m_left > 0) {
            buildSection();                      // yksi osio per kehys (puhelin ei jaady)
            m_left--;
            m_done++;
        }
        if (m_left <= 0) {
            s_active = false;
            FLAlertLayer::create("Remix",
                fmt::format("Added {} objects in {} sections.\n"
                            "Playtest the transitions before using the level.",
                            m_made, m_done).c_str(), "OK")->show();
            this->unscheduleUpdate();
            this->removeFromParentAndCleanup(true);
        }
    }

public:
    static bool active() { return s_active; }
    static RemixRunner* create(LevelEditorLayer* lel, RemixSettings const& rs) {
        auto ret = new RemixRunner();
        if (ret->init(lel, rs)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ---------- Remix-valikko: valitse mista leveleista kopioidaan ----------
class RemixPopup : public geode::Popup {
protected:
    static constexpr int PER_PAGE = 5;

    LevelEditorLayer* m_lel = nullptr;
    std::vector<GJGameLevel*> m_levels;
    std::vector<bool> m_selected;
    std::vector<CCMenuItemToggler*> m_rowToggles;     // nykyisen sivun togglet
    std::vector<int> m_rowIndex;                      // niiden level-indeksit
    std::vector<CCNode*> m_rowNodes;                  // poistettavat solmut
    int m_page = 0;
    CCLabelBMFont* m_pageLabel = nullptr;
    Slider* m_secSlider = nullptr;
    Slider* m_varSlider = nullptr;
    CCLabelBMFont* m_secLabel = nullptr;
    CCLabelBMFont* m_varLabel = nullptr;

    int pageCount() const {
        return std::max(1, (static_cast<int>(m_levels.size()) + PER_PAGE - 1) / PER_PAGE);
    }
    int sections() { return 1 + static_cast<int>(std::round(m_secSlider->getValue() * 19)); }
    int variation() { return static_cast<int>(std::round(m_varSlider->getValue() * 100)); }

    void refreshLabels() {
        m_secLabel->setString(fmt::format("Sections: {}", sections()).c_str());
        m_varLabel->setString(fmt::format("Variation: {}%", variation()).c_str());
    }
    void onSlider(CCObject*) { refreshLabels(); }
    void onToggle(CCObject*) {}

    void saveCurrentPage() {
        for (size_t i = 0; i < m_rowToggles.size(); i++)
            m_selected[m_rowIndex[i]] = m_rowToggles[i]->isToggled();
    }

    void rebuildPage() {
        for (auto n : m_rowNodes) n->removeFromParent();
        m_rowNodes.clear();
        m_rowToggles.clear();
        m_rowIndex.clear();

        float h = m_size.height;
        int first = m_page * PER_PAGE;
        for (int i = 0; i < PER_PAGE; i++) {
            int idx = first + i;
            if (idx >= static_cast<int>(m_levels.size())) break;
            float y = h - 62.f - 28.f * i;

            auto t = CCMenuItemToggler::createWithStandardSprites(
                this, menu_selector(RemixPopup::onToggle), 0.6f);
            t->setPosition({40.f, y});
            t->toggle(m_selected[idx]);
            m_buttonMenu->addChild(t);
            m_rowNodes.push_back(t);
            m_rowToggles.push_back(t);
            m_rowIndex.push_back(idx);

            std::string name = m_levels[idx]->m_levelName;
            if (name.size() > 18) name = name.substr(0, 17) + ".";
            if (name.empty()) name = "(unnamed)";
            auto l = CCLabelBMFont::create(name.c_str(), "bigFont.fnt");
            l->setScale(0.4f);
            l->setAnchorPoint({0.f, 0.5f});
            l->setPosition({62.f, y});
            m_mainLayer->addChild(l);
            m_rowNodes.push_back(l);
        }
        m_pageLabel->setString(fmt::format("{}/{}", m_page + 1, pageCount()).c_str());
    }

    void onPrev(CCObject*) {
        saveCurrentPage();
        m_page = (m_page + pageCount() - 1) % pageCount();
        rebuildPage();
    }
    void onNext(CCObject*) {
        saveCurrentPage();
        m_page = (m_page + 1) % pageCount();
        rebuildPage();
    }

    void onRemix(CCObject*) {
        if (RemixRunner::active()) return;
        saveCurrentPage();
        RemixSettings rs;
        for (size_t i = 0; i < m_levels.size(); i++)
            if (m_selected[i]) rs.levels.push_back(m_levels[i]);
        if (rs.levels.empty()) {
            Notification::create("Select at least one level", NotificationIcon::Error, 1.5f)->show();
            return;
        }
        rs.sections = sections();
        rs.variation = variation();
        auto runner = RemixRunner::create(m_lel, rs);
        if (!runner) {
            Notification::create("Could not read the selected levels", NotificationIcon::Error, 2.f)->show();
            return;
        }
        m_lel->addChild(runner);
        Notification::create("Remixing...", NotificationIcon::Loading, 1.f)->show();
        this->onClose(nullptr);
    }

    bool init(LevelEditorLayer* lel) {
        if (!Popup::init(400.f, 280.f)) return false;
        m_lel = lel;
        this->setTitle("Remix - pick your levels");

        auto arr = LocalLevelManager::sharedState()->m_localLevels;
        if (arr) {
            CCObject* o = nullptr;
            CCARRAY_FOREACH(arr, o) {
                auto lvl = static_cast<GJGameLevel*>(o);
                if (lvl && lvl != lel->m_level) m_levels.push_back(lvl);   // ei kopioida itsesta
            }
        }
        m_selected.assign(m_levels.size(), true);
        float h = m_size.height;

        if (m_levels.empty()) {
            auto none = CCLabelBMFont::create("No other levels found", "bigFont.fnt");
            none->setScale(0.5f);
            none->setPosition({m_size.width / 2, h / 2});
            m_mainLayer->addChild(none);
            return true;
        }

        m_pageLabel = CCLabelBMFont::create("1/1", "bigFont.fnt");
        m_pageLabel->setScale(0.45f);
        m_pageLabel->setPosition({120.f, 85.f});
        m_mainLayer->addChild(m_pageLabel);

        auto leftSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        leftSpr->setScale(0.6f);
        auto prev = CCMenuItemSpriteExtra::create(leftSpr, this, menu_selector(RemixPopup::onPrev));
        prev->setPosition({60.f, 85.f});
        m_buttonMenu->addChild(prev);

        auto rightSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        rightSpr->setScale(0.6f);
        rightSpr->setFlipX(true);
        auto next = CCMenuItemSpriteExtra::create(rightSpr, this, menu_selector(RemixPopup::onNext));
        next->setPosition({180.f, 85.f});
        m_buttonMenu->addChild(next);

        m_secLabel = CCLabelBMFont::create("", "bigFont.fnt");
        m_secLabel->setScale(0.4f);
        m_secLabel->setPosition({320.f, h - 80.f});
        m_mainLayer->addChild(m_secLabel);
        m_secSlider = Slider::create(this, menu_selector(RemixPopup::onSlider), 0.45f);
        m_secSlider->setPosition({320.f, h - 100.f});
        m_secSlider->setValue(0.25f);
        m_mainLayer->addChild(m_secSlider);

        m_varLabel = CCLabelBMFont::create("", "bigFont.fnt");
        m_varLabel->setScale(0.4f);
        m_varLabel->setPosition({320.f, h - 140.f});
        m_mainLayer->addChild(m_varLabel);
        m_varSlider = Slider::create(this, menu_selector(RemixPopup::onSlider), 0.45f);
        m_varSlider->setPosition({320.f, h - 160.f});
        m_varSlider->setValue(0.4f);
        m_mainLayer->addChild(m_varSlider);

        auto remix = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Remix"), this, menu_selector(RemixPopup::onRemix));
        remix->setPosition({320.f, 50.f});
        m_buttonMenu->addChild(remix);

        refreshLabels();
        rebuildPage();
        return true;
    }

public:
    static RemixPopup* create(LevelEditorLayer* lel) {
        auto ret = new RemixPopup();
        if (ret->init(lel)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ============================================================
//  NAPPIT EDITORIIN
// ============================================================
class $modify(AutoEditorUI, EditorUI) {
    bool init(LevelEditorLayer* lel) {
        if (!EditorUI::init(lel)) return false;

        auto menu = CCMenu::create();
        auto autoBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Auto"), this, menu_selector(AutoEditorUI::onAuto));
        auto remixBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Remix"), this, menu_selector(AutoEditorUI::onRemix));
        menu->addChild(autoBtn);
        menu->addChild(remixBtn);
        menu->alignItemsHorizontallyWithPadding(8.f);
        menu->setPosition({110.f, CCDirector::get()->getWinSize().height - 40.f});
        this->addChild(menu);
        return true;
    }

    void onAuto(CCObject*) {
        AutoBuildPopup::create(m_editorLayer)->show();
    }
    void onRemix(CCObject*) {
        if (auto p = RemixPopup::create(m_editorLayer)) p->show();
    }
};
