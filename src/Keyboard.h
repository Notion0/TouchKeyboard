// Keyboard/Keyboard.h
// 触摸键盘主面板：字母/符号/中文候选输入，经 InputManager 全局挂载到行编辑。
#ifndef AEA_KEYBOARD_H
#define AEA_KEYBOARD_H

#include "AbstractKeyboard.h"
#include "KeyButton.h"
#include <QLayout>
#include <QListWidget>
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
    ChineseWidget(QWidget *parent = NULL);
    /// @brief 设置拼音缓冲文本
    void setText(const QString &text);

signals:
    /// @brief 按键信号（码+文本），供目标行编辑接收
    void pressedChanged(int code, QString text);

private slots:
    /// @brief 候选词点击：上屏并推进拼音
    void onItemClicked(QListWidgetItem *item);

private:
    /// @brief 追加一个候选词
    void addOneItem(const QString &text);
    /// @brief 加载字库（按 qrc 宏启用）
    void loadChineseLib();
    /// @brief 加载词组库（按 qrc 宏启用）
    void loadChinesePhraseLib();
    /// @brief 加载谷歌汉字库（默认启用）
    void loadGoogleChineseLib();

private:
    QMap<QString, QList<QPair<QString, QString> > > m_data;
};

class Keyboard : public AbstractKeyboard
{
    Q_OBJECT
public:
    /// @brief 键盘构造：构建按键布局
    Keyboard(QWidget *parent = NULL);
    ~Keyboard();  // ========== 新增析构函数 ==========

protected:
    /// @brief 尺寸变化：重排按键
    void resizeEvent(QResizeEvent *e);

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

private:
    /// @brief 造一个多态按键
    KeyButton *createButton(QList<KeyButton::Mode> modes);

    /// @brief 造一行按键
    QWidget *createBar(const QList<QList<KeyButton::Mode> > &modes);
    /// @brief 中文输入行（拼音+候选）
    QWidget *chineseBar();

    /// @brief 候选词列表区
    QWidget *candidateList();

    /// @brief 按容器尺寸重算按键几何
    void resizeButton();

private:
    Ui::Keyboard *ui;
    bool m_isChinese;
    ChineseWidget *m_chineseWidget;
    QString m_bufferText;

signals:
    /// @brief 按键信号（码+文本），供目标行编辑接收
    void pressedChanged(int code, QString text);

};

}
#endif // AEA_KEYBOARD_H
