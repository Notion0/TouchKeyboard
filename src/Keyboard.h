// Keyboard/Keyboard.h
// 触摸键盘主面板：字母/符号/中文候选输入——按键经 AbstractKeyboard::onKeyPressed
// 以 QKeyEvent 送达焦点控件；弹/收与选键盘由 InputManager 按焦点驱动。
#ifndef AEA_KEYBOARD_H
#define AEA_KEYBOARD_H

#include "AbstractKeyboard.h"
#include "KeyButton.h"
#include <QLayout>
#include <QListWidget>
#include <QStyledItemDelegate>
#include <QAbstractItemDelegate>
#include <QHash>
#include <QPair>
#include <QString>

// ========== 新增：前置声明 UI 类 ==========
namespace Ui {
class Keyboard;
}
// =======================================

namespace AeaQt {

class ChineseWidget : public QListWidget {
    Q_OBJECT
public:
    /// @brief 中文候选部件构造
    ChineseWidget(QWidget *parent = nullptr);
    /// @brief 设置拼音缓冲文本
    void setText(const QString &text);

    /// @brief 候选条目像素字号（宿主按字号系数统一下发；0 = 用模板默认值）
    void setCapPixels(int px);

signals:
    /// @brief 按键信号（码+文本），供目标行编辑接收
    void pressedChanged(int code, QString text);

private slots:
    /// @brief 候选词点击：上屏并推进拼音
    void onItemClicked(QListWidgetItem *item);

private:
    /// @brief 追加一个候选词
    void addOneItem(const QString &text);
    /// @brief 候选条目当前像素字号（0 = 模板默认）
    int capPixels() const { return m_capPixels; }
    /// @brief 加载单字拼音库（资源路径见 TOUCHKBD_PINYIN_DICT）
    void loadChineseLib();
    /// @brief 加载词组库（资源路径见 TOUCHKBD_PINYIN_PHRASE_DICT）
    void loadChinesePhraseLib();
    /// @brief 加载谷歌大字典（资源路径见 TOUCHKBD_GOOGLE_DICT）
    void loadGoogleChineseLib();

private:
    QMap<QString, QList<QPair<QString, QString> > > m_data;
    int m_capPixels = 0;   // 0 = 未下发（用 kCandidateFontPixels）

protected:
    /// @brief 拖动期间吞掉 mouseRelease（QScroller 已识别为拖动时不当点击）
    bool eventFilter(QObject *watched, QEvent *e) override;
};

/// 候选条目自绘 delegate：禁 QSS 后 QListWidget::Item:hover/selected 的替代。
/// 圆角高亮底 + 同族文字色（取色见 AeaKeyboardStyle.h 的 candidate* 常量）。
class CandidateItemDelegate : public QStyledItemDelegate {
public:
    explicit CandidateItemDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
};

class Keyboard : public AbstractKeyboard
{
    Q_OBJECT
public:
    /// @brief 键盘构造：构建按键布局
    Keyboard(QWidget *parent = nullptr);
    ~Keyboard();  // ========== 新增析构函数 ==========

    /// @brief 英文-only 模式：为 true 时强制停靠英文态，且中/英切换键置灰失效。
    ///        由 InputManager 按输入框的 asciiOnly 动态属性调用——密码等仅允许
    ///        ASCII 的字段若不禁用中文态，触摸键盘的拼音候选能点选却过不了字段的
    ///        validator，表现为"中文能显示但不能选中写入"
    void setEnglishOnly(bool enabled) override;
    bool isChineseMode() const override;
    void setChineseMode(bool chinese) override;

    /// @brief 语言切换后刷新键帽文案（宿主 installTranslator 之后调；changeEvent 也会触发）
    void retranslate() override;
    /// @brief 键帽像素字号下发：KeyButton 键帽 + 候选条目字号一并走
    void setKeyCapPixels(int px) override;

protected:
    /// @brief 尺寸变化：重排按键 + 重算圆角 mask
    void resizeEvent(QResizeEvent *e) override;
    /// @brief 收起：丢弃未提交的拼音缓冲与候选（缓冲跨输入框存活，不清会串台）
    void hideEvent(QHideEvent *e) override;
    /// @brief 面板自绘（禁 QSS 后 background/border-radius/border 的替代）
    void paintEvent(QPaintEvent *e) override;

private slots:
    /// @brief 大小写锁定切换
    void switchCapsLock();
    /// @brief 符号页切换
    void switchSpecialChar();
    /// @brief 中英文输入切换
    void switchEnOrCh();
    /// @brief 按键按下分发（含功能键）
    void onButtonPressed(const int &code, const QString &text);
    /// @brief 清空拼音缓冲
    void clearBufferText();
    /// @brief 候选词上屏：焦点控件直接插入，其余控件兜底合成键事件
    void onCandidateCommitted(const int &code, const QString &text);

private:
    /// @brief 造一个多态按键
    KeyButton *createButton(QList<KeyButton::Mode> modes);

    /// @brief 按容器尺寸重算按键几何
    void resizeButton();
    /// @brief 底两行 stretch 与总份数：功能键统一规格表的基准宽换算而来
    void applyRowStretches();
    /// @brief 键帽文案（tr()）单一出处：构造、切符号页、语言切换都走这里
    void applyKeyLabels();
    /// @brief 标题栏收起钮：图标/命中区/尺寸
    void updateCloseButton();
    void fitKeyFont(KeyButton *button, int basePx, bool bold, int stretch);

    /// @brief 按 m_isChinese 刷新候选区可见性（中文态常驻，英文态收起让位标题栏）
    void updateLangState();

private:
    Ui::Keyboard *ui;
    bool m_isChinese;
    bool m_englishOnly = false;   // 英文-only：中/英切换失效（密码等 ASCII-only 字段）
    bool m_isSpecialChar = false; // 当前是否符号/数字页（驱动左下角切换键键帽文案）
    ChineseWidget *m_chineseWidget;
    QString m_bufferText;
    // 底两行布局与总份数（功能键统一宽换算后写回这里；fitKeyFont 按它算可用宽）
    QHBoxLayout *m_row3Layout = nullptr;
    QHBoxLayout *m_row4Layout = nullptr;
    int m_row3TotalStretch = 0;
    int m_row4TotalStretch = 0;
};

}
#endif // AEA_KEYBOARD_H
