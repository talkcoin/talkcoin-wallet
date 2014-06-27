#ifndef MESSAGINGPAGE_H
#define MESSAGINGPAGE_H

#include <QWidget>

namespace Ui {
    class MessagingPage;
}

/** Messaging page widget */
class MessagingPage : public QWidget
{
    Q_OBJECT

public:
    explicit MessagingPage(QWidget *parent = 0);
    ~MessagingPage();

protected:

public slots:

signals:

private:
    Ui::MessagingPage *ui;

private slots:

};

#endif // MESSAGINGPAGE_H
