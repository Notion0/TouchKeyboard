// Keyboard/T9Keyboard.h
// 九宫格文本键盘：中文九宫格拼音 / 英文数字直选+词组候选 / 数字 / 符号四种模式。
// 源自 aerocount 分叉线（docs/17, L7, 2026-10-22），2026-10-26 合回模板，与 26 键 Keyboard
// 并存：末行「全键盘」键发 layoutSwitchRequested，由 InputManager 切换。
// 继承 AbstractKeyboard：按键经 onKeyPressed 以 QKeyEvent 送达焦点控件；
// 弹出/收起/定位仍由 InputManager 负责。全部自绘，不用 setStyleSheet。
#ifndef AEA_T9_KEYBOARD_H
#define AEA_T9_KEYBOARD_H

#include "AbstractKeyboard.h"
#include "T9PinyinEngine.h"

#include <QAbstractButton>

namespace AeaQt {

// 自绘键帽：主文字居中，副文字（如 "ABC"）在下方
class T9Key : public QAbstractButton
{
    Q_OBJECT
public:
    enum Role { Normal, Function };
    explicit T9Key(QWidget *parent = nullptr);
    void setLabels(const QString &main, const QString &sub = QString());
    void setRole(Role role);
    void setActive(bool on);   // 当前模式高亮
    QSize sizeHint() const override { return QSize(96, 64); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_sub;
    Role m_role = Normal;
    bool m_active = false;
};

// 可拖动滑动的条目条：拼音组合栏、候选栏（横向）与标点列（纵向）共用。
// 拖动超过阈值即滚动，未拖动的松开才算点击
class T9Strip : public QWidget
{
    Q_OBJECT
public:
    T9Strip(Qt::Orientation orientation, QWidget *parent = nullptr);
    void setItems(const QStringList &items);
    const QStringList &items() const { return m_items; }
    void setPlaceholder(const QString &text);   // 无条目时显示的灰字，不可点
    void setHighlight(int index);
    void setFontPixelSize(int px);
    void setColors(const QColor &background, const QColor &text, const QColor &accent);

signals:
    void itemClicked(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void relayoutItems();
    int axis(const QPoint &p) const;
    int viewLength() const;
    int maxScroll() const;
    int itemAt(const QPoint &p) const;

    Qt::Orientation m_orientation;
    QStringList m_items;
    QList<int> m_starts;
    QList<int> m_lengths;
    QString m_placeholder;
    int m_highlight = -1;
    int m_scroll = 0;
    int m_pressAxis = 0;
    int m_pressScroll = 0;
    bool m_pressed = false;
    bool m_dragging = false;
    int m_fontPx = 20;
    QColor m_background;
    QColor m_text;
    QColor m_accent;
};

class T9Keyboard : public AbstractKeyboard
{
    Q_OBJECT
public:
    enum Mode { Chinese, English, Digits, Symbols };

    explicit T9Keyboard(QWidget *parent = nullptr);
    ~T9Keyboard() override;

    /// @brief 宿主注入业务词（账户名/房间名/点位名等），在键盘弹出时机设置
    void setExtraWords(const QStringList &words);

    /// @brief 英文-only（asciiOnly 字段）：停靠英文模式，中/英键置灰
    void setEnglishOnly(bool enabled) override;
    bool isChineseMode() const override;
    void setChineseMode(bool chinese) override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    // 网格键位编号（位置固定，文案随模式变）
    enum KeyId {
        KeySymbol, KeyNum,
        KeyD1, KeyD2, KeyD3, KeyD4, KeyD5, KeyD6, KeyD7, KeyD8, KeyD9,
        KeyLang, KeyD0, KeyLayout,
        KeyBack, KeyClear, KeyEnter,
        KeyCount
    };

    T9Key *addKey(int id);
    void layoutKeys();
    void applyMode();
    void setMode(Mode mode);

    void onKey(int id);
    void onDigit(int digit);
    void onEnglishDigit(int digit);
    void onCompositionClicked(int index);
    void onCandidateClicked(int index);
    void onPunctClicked(int index);
    void onSymbolClicked(int index);

    void commitText(const QString &text);
    void commitEnglishItem(const QString &item, bool isWord);
    bool commitFirstCandidate();
    void sendBackspace();
    void resetComposition();
    void refreshChinese();
    void refreshEnglish();
    void updateEcho();
    QStringList matchWords(const QByteArray &seq) const;
    QByteArray encodeWord(const QString &word) const;
    void rebuildWords();
    void learnWord(const QString &word);
    void learnPendingWord();
    void loadLearned();
    void saveLearned() const;
    QStringList punctuation() const;
    QStringList symbolPage() const;

    T9PinyinEngine *m_engine = nullptr;
    T9Strip *m_compBar = nullptr;
    T9Strip *m_candBar = nullptr;
    T9Strip *m_punctStrip = nullptr;
    QList<T9Key *> m_keys;          // 下标即 KeyId
    QList<T9Key *> m_symbolKeys;    // 符号页网格

    Mode m_mode = Chinese;
    Mode m_textMode = Chinese;      // 从数字/符号页返回时回到的文本模式
    bool m_englishOnly = false;     // asciiOnly 字段：禁止进入中文模式
    int m_symbolPage = 0;

    QByteArray m_input;             // 中文：数字与分词符 '
    QByteArray m_engInput;          // 英文：已按数字序列（候选匹配用）
    QString m_typedWord;            // 英文：已直选的字母（到达词边界时学习）
    bool m_lastTargetPassword = false;  // 最后一次上屏的目标是不是密码框（学词判据）
    int m_engWordCount = 0;         // 英文：候选栏前多少项是词（其余为字母，密码场景）
    bool m_echoActive = false;      // 密码框回显中（焦点是 Password 模式输入框）
    QString m_echoText;             // 回显内容（字段真实文本，仅键盘上可见）
    QStringList m_learned;          // 学过的词（QSettings 持久化，不自动清理）
    QStringList m_extra;            // 宿主注入的业务词
    QVector<QPair<QString, QString>> m_words;  // (词, 数字编码)，按 学习→业务→内置 排序
    QString m_locked;               // 组合栏锁定的拼音（去分词符）
    QStringList m_comps;
    QList<T9Candidate> m_cands;
};

}

#endif // AEA_T9_KEYBOARD_H
