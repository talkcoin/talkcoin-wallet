#include "messagingpage.h"
#include "ui_messagingpage.h"

MessagingPage::MessagingPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MessagingPage)
{
    ui->setupUi(this);
}

MessagingPage::~MessagingPage()
{
    delete ui;
}
