#pragma once

#include <fcitx/inputmethodengine.h>
#include <fcitx/addoninstance.h>
#include <fcitx/addonfactory.h>
#include <fcitx/instance.h>
#include <string>
#include <vector>
#include "abk/abk.h"

namespace fcitx {

class AmarBanglaKothaEngine : public InputMethodEngineV2 {
public:
    AmarBanglaKothaEngine(Instance* instance);
    ~AmarBanglaKothaEngine() override;

    void keyEvent(const InputMethodEntry& entry, KeyEvent& keyEvent) override;
    void reset(const InputMethodEntry& entry, InputContextEvent& event) override;

    void onCandidateClicked(InputContext* ic, size_t index, const std::string& word);

private:
    void updatePreedit(InputContext* ic, const std::string& buffer);
    void commitWord(InputContext* ic, const std::string& word, bool appendSpace);
    void cancelComposition(InputContext* ic);

    Instance* instance_;
    abk_engine_t engine_;
    std::string buffer_;
    std::string primaryText_;
    std::vector<std::string> candidates_;
    int selectedIndex_;
};

class AmarBanglaKothaFactory : public AddonFactory {
public:
    AddonInstance* create(AddonManager* manager) override;
};

} // namespace fcitx
