#include "abk_fcitx5.h"
#include <fcitx/candidatelist.h>
#include <fcitx/inputcontext.h>
#include <fcitx-utils/standardpath.h>
#include <cstdlib>

namespace fcitx {

class AbkCandidateWord : public CandidateWord {
public:
    AbkCandidateWord(AmarBanglaKothaEngine* engine, size_t index, const std::string& word)
        : CandidateWord(Text(word)), engine_(engine), index_(index), word_(word) {}

    void select(InputContext* ic) const override {
        if (engine_) {
            engine_->onCandidateClicked(ic, index_, word_);
        }
    }

private:
    AmarBanglaKothaEngine* engine_;
    size_t index_;
    std::string word_;
};

AmarBanglaKothaEngine::AmarBanglaKothaEngine(Instance* instance)
    : instance_(instance), engine_(nullptr), selectedIndex_(0) {
    engine_ = abk_engine_create();

    // Locate dictionary
    std::string homeDir = getenv("HOME") ? getenv("HOME") : "";
    std::vector<std::string> dictCandidates = {
        homeDir + "/.local/share/amar-bangla-kotha/bangla_wordlist.txt",
        "/usr/local/share/amar-bangla-kotha/bangla_wordlist.txt",
        "/usr/share/amar-bangla-kotha/bangla_wordlist.txt"
    };

    for (const auto& path : dictCandidates) {
        if (abk_load_dictionary(engine_, path.c_str()) == 1) {
            break;
        }
    }
}

AmarBanglaKothaEngine::~AmarBanglaKothaEngine() {
    if (engine_) {
        abk_engine_destroy(engine_);
        engine_ = nullptr;
    }
}

void AmarBanglaKothaEngine::updatePreedit(InputContext* ic, const std::string& buffer) {
    if (buffer.empty()) {
        primaryText_.clear();
        candidates_.clear();
        selectedIndex_ = 0;
        if (engine_) {
            abk_suggestion_reset(engine_);
        }
        ic->inputPanel().reset();
        ic->updatePreedit();
        ic->updateUserInterface(UserInterfaceComponent::InputPanel);
        return;
    }

    abk_suggestion_list_t* res = abk_get_suggestions(engine_, buffer.c_str());
    if (!res) return;

    primaryText_ = res->primary_transliteration ? res->primary_transliteration : buffer;
    candidates_.clear();
    for (size_t i = 0; i < res->candidate_count; ++i) {
        if (res->candidates[i].word) {
            candidates_.push_back(res->candidates[i].word);
        }
    }
    selectedIndex_ = abk_suggestion_get_selected_index(engine_);
    abk_free_suggestions(res);

    // Update preedit string
    Text preedit;
    preedit.append(primaryText_, TextFormatFlag::Underline);
    ic->inputPanel().setPreedit(preedit);

    // Update candidate list with interactive selectable words
    if (!candidates_.empty()) {
        auto candidateList = std::make_unique<CommonCandidateList>();
        for (size_t i = 0; i < candidates_.size(); ++i) {
            candidateList->append(std::make_unique<AbkCandidateWord>(this, i, candidates_[i]));
        }
        candidateList->setCursor(selectedIndex_);
        ic->inputPanel().setCandidateList(std::move(candidateList));
    } else {
        ic->inputPanel().setCandidateList(nullptr);
    }

    ic->updatePreedit();
    ic->updateUserInterface(UserInterfaceComponent::InputPanel);
}

void AmarBanglaKothaEngine::onCandidateClicked(InputContext* ic, size_t index, const std::string& word) {
    selectedIndex_ = (int)index;
    abk_suggestion_select_index(engine_, (int)index);
    abk_log_event(engine_, "CLICK", word.c_str());
    commitWord(ic, word, true);
}

void AmarBanglaKothaEngine::commitWord(InputContext* ic, const std::string& word, bool appendSpace) {
    std::string commitStr = appendSpace ? (word + " ") : word;
    ic->commitString(commitStr);

    buffer_.clear();
    primaryText_.clear();
    candidates_.clear();
    selectedIndex_ = 0;
    if (engine_) {
        abk_suggestion_reset(engine_);
    }
    ic->inputPanel().reset();
    ic->updatePreedit();
    ic->updateUserInterface(UserInterfaceComponent::InputPanel);
}

void AmarBanglaKothaEngine::cancelComposition(InputContext* ic) {
    buffer_.clear();
    primaryText_.clear();
    candidates_.clear();
    selectedIndex_ = 0;
    if (engine_) {
        abk_suggestion_reset(engine_);
    }
    ic->inputPanel().reset();
    ic->updatePreedit();
    ic->updateUserInterface(UserInterfaceComponent::InputPanel);
}

void AmarBanglaKothaEngine::reset(const InputMethodEntry& /*entry*/, InputContextEvent& event) {
    cancelComposition(event.inputContext());
}

void AmarBanglaKothaEngine::keyEvent(const InputMethodEntry& /*entry*/, KeyEvent& keyEvent) {
    if (keyEvent.isRelease()) return;

    auto* ic = keyEvent.inputContext();
    if (!ic) return;

    // Workstream 2: Auto-detect password / PIN / sensitive fields
    if (ic->capabilityFlags().test(CapabilityFlag::Password) ||
        ic->capabilityFlags().test(CapabilityFlag::Sensitive)) {
        abk_engine_set_secure_mode(engine_, 1);
        if (!buffer_.empty()) {
            cancelComposition(ic);
        }
        return; // Auto-bypass, pass keys directly to host application without learning or logging
    } else {
        abk_engine_set_secure_mode(engine_, 0);
    }

    // Pass through Ctrl / Alt / Super shortcuts
    if (keyEvent.rawKey().hasModifier() &&
        (keyEvent.rawKey().checkModifier(KeyModifier::Ctrl) ||
         keyEvent.rawKey().checkModifier(KeyModifier::Alt) ||
         keyEvent.rawKey().checkModifier(KeyModifier::Super))) {
        if (!buffer_.empty()) {
            commitWord(keyEvent.inputContext(), primaryText_, false);
        }
        return;
    }

    Key sym = keyEvent.rawKey().sym();
    auto* ic = keyEvent.inputContext();

    // 1. Navigation keys (Down, Up, Tab)
    if (!buffer_.empty() && !candidates_.empty()) {
        if (sym == FcitxKey_Down) {
            abk_suggestion_select_next(engine_);
            selectedIndex_ = abk_suggestion_get_selected_index(engine_);
            abk_log_event(engine_, "KEY_DOWN", "navigate next candidate");
            updatePreedit(ic, buffer_);
            keyEvent.filterAndAccept();
            return;
        }
        if (sym == FcitxKey_Up) {
            abk_suggestion_select_prev(engine_);
            selectedIndex_ = abk_suggestion_get_selected_index(engine_);
            abk_log_event(engine_, "KEY_UP", "navigate prev candidate");
            updatePreedit(ic, buffer_);
            keyEvent.filterAndAccept();
            return;
        }
        if (sym == FcitxKey_Tab) {
            if (keyEvent.rawKey().checkModifier(KeyModifier::Shift)) {
                abk_suggestion_select_prev(engine_);
                abk_log_event(engine_, "KEY_SHIFT_TAB", "navigate prev candidate");
            } else {
                abk_suggestion_select_next(engine_);
                abk_log_event(engine_, "KEY_TAB", "navigate next candidate");
            }
            selectedIndex_ = abk_suggestion_get_selected_index(engine_);
            updatePreedit(ic, buffer_);
            keyEvent.filterAndAccept();
            return;
        }
    }

    // 2. Escape key
    if (sym == FcitxKey_Escape) {
        if (!buffer_.empty()) {
            abk_log_event(engine_, "KEY_ESCAPE", "cancel composition");
            cancelComposition(ic);
            keyEvent.filterAndAccept();
            return;
        }
        return;
    }

    // 3. Space key
    if (sym == FcitxKey_space) {
        if (!buffer_.empty()) {
            const char* sel = abk_suggestion_get_selected_word_ptr(engine_);
            std::string target = (sel && strlen(sel) > 0) ? sel : (!candidates_.empty() ? candidates_[0] : primaryText_);
            abk_log_event(engine_, "KEY_SPACE", target.c_str());
            commitWord(ic, target, true);
            keyEvent.filterAndAccept();
            return;
        }
        return;
    }

    // 4. Return / Enter key
    if (sym == FcitxKey_Return || sym == FcitxKey_KP_Enter) {
        if (!buffer_.empty()) {
            const char* sel = abk_suggestion_get_selected_word_ptr(engine_);
            std::string target = (sel && strlen(sel) > 0) ? sel : (!candidates_.empty() ? candidates_[0] : primaryText_);
            abk_log_event(engine_, "KEY_ENTER", target.c_str());
            commitWord(ic, target, false);
            keyEvent.filterAndAccept();
            return;
        }
        return;
    }

    // 5. Backspace
    if (sym == FcitxKey_BackSpace) {
        if (!buffer_.empty()) {
            buffer_.pop_back();
            updatePreedit(ic, buffer_);
            keyEvent.filterAndAccept();
            return;
        }
        return;
    }

    // 6. Candidate selection via 1-9
    if (!buffer_.empty() && sym >= FcitxKey_1 && sym <= FcitxKey_9) {
        size_t idx = static_cast<size_t>(sym - FcitxKey_1);
        if (idx < candidates_.size()) {
            selectedIndex_ = (int)idx;
            abk_suggestion_select_index(engine_, (int)idx);
            abk_log_event(engine_, "KEY_NUMBER", candidates_[idx].c_str());
            commitWord(ic, candidates_[idx], true);
            keyEvent.filterAndAccept();
            return;
        }
    }
            commitWord(ic, candidates_[idx], true);
            keyEvent.filterAndAccept();
            return;
        }
    }

    // 6. Phonetic Latin characters
    if (sym >= 32 && sym < 127) {
        char ch = static_cast<char>(sym);
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
            ch == '^' || ch == ':' || ch == '`') {
            buffer_.push_back(ch);
            updatePreedit(ic, buffer_);
            keyEvent.filterAndAccept();
            return;
        }

        // Punctuation
        if (!buffer_.empty() && (ch == '.' || ch == ',' || ch == '!' || ch == '?' ||
                                 ch == ';' || ch == ':' || ch == '-' || ch == '(' || ch == ')')) {
            std::string target = !candidates_.empty() ? candidates_[0] : primaryText_;
            std::string punctStr = (ch == '.') ? "।" : std::string(1, ch);
            commitWord(ic, target + punctStr, false);
            keyEvent.filterAndAccept();
            return;
        }
    }
}

AddonInstance* AmarBanglaKothaFactory::create(AddonManager* /*manager*/) {
    return new AmarBanglaKothaEngine(instance_);
}

} // namespace fcitx

FCITX_ADDON_FACTORY(fcitx::AmarBanglaKothaFactory);
