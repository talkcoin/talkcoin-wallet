#include "overviewpage.h"
#include "ui_overviewpage.h"

#include "clientmodel.h"
#include "walletmodel.h"
#include "talkcoinunits.h"
#include "optionsmodel.h"
#include "transactiontablemodel.h"
#include "transactionfilterproxy.h"
#include "guiutil.h"
#include "guiconstants.h"

#include <QAbstractItemDelegate>
#include <QPainter>

#include <QTimer>
#include <QKeyEvent>
#include <QScrollBar>
#include "coincontroldialog.h"
#include "util.h"
#include "base58.h"

#define DECORATION_SIZE 64
#define NUM_ITEMS 3

extern int nBestHeight;
extern int64 nSubsidy;
extern int V_BlockInterval;
extern int V_BlockInit;
extern int V_Blocks;
extern int V_Total;

static const unsigned int tlk_size1 = 50;
static const unsigned int tlk_size2 = 6;

static std::string TLK___[tlk_size1][tlk_size2];
static std::string _tlk;

extern std::string TLK_en[tlk_size1][tlk_size2];
extern std::string TLK_ru[tlk_size1][tlk_size2];
extern std::string TLK_cn[tlk_size1][tlk_size2];

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    TxViewDelegate(): QAbstractItemDelegate(), unit(TalkcoinUnits::TAC)
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QRect mainRect = option.rect;
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
        int xspace = DECORATION_SIZE + 8;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
        if(value.canConvert<QBrush>())
        {
            QBrush brush = qvariant_cast<QBrush>(value);
            foreground = brush.color();
        }

        painter->setPen(foreground);
        painter->drawText(addressRect, Qt::AlignLeft|Qt::AlignVCenter, address);

        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if(!confirmed)
        {
            foreground = COLOR_UNCONFIRMED;
        }
        else
        {
            foreground = option.palette.color(QPalette::Text);
        }
        painter->setPen(foreground);
        QString amountText = TalkcoinUnits::formatWithUnit(unit, amount, true);
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight|Qt::AlignVCenter, amountText);

        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(amountRect, Qt::AlignLeft|Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        return QSize(DECORATION_SIZE, DECORATION_SIZE);
    }

    int unit;

};
#include "overviewpage.moc"

OverviewPage::OverviewPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OverviewPage),
    clientModel(0),
    walletModel(0),
    currentBalance(-1),
    currentUnconfirmedBalance(-1),
    currentImmatureBalance(-1),
    txdelegate(new TxViewDelegate()),
    filter(0)
{
    ui->setupUi(this);

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));

    // init "out of sync" warning labels
    ui->labelWalletStatus->setText("(" + tr("out of sync") + ")");
    ui->labelTransactionsStatus->setText("(" + tr("out of sync") + ")");

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);
}

void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
    if(filter)
        emit transactionClicked(filter->mapToSource(index));
}

OverviewPage::~OverviewPage()
{
    delete ui;
}

void OverviewPage::setBalance(qint64 balance, qint64 unconfirmedBalance, qint64 immatureBalance)
{
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    currentBalance = balance;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;
    ui->labelBalance->setText(TalkcoinUnits::formatWithUnit(unit, balance));
    ui->labelUnconfirmed->setText(TalkcoinUnits::formatWithUnit(unit, unconfirmedBalance));
    ui->labelImmature->setText(TalkcoinUnits::formatWithUnit(unit, immatureBalance));

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showImmature = immatureBalance != 0;
    ui->labelImmature->setVisible(showImmature);
    ui->labelImmatureText->setVisible(showImmature);
}

void OverviewPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model)
    {
        // Show warning if this is a prerelease version
        connect(model, SIGNAL(alertsChanged(QString)), this, SLOT(updateAlerts(QString)));
        updateAlerts(model->getStatusBarWarnings());
    }


#if QT_VERSION >= 0x040700
    /* Do not move this to the XML file, Qt before 4.7 will choke on it */
    ui->txtChatNick->setPlaceholderText(tr("Nickname"));
    ui->txtChatMsg->setPlaceholderText(tr("Type Your Message Here!"));
#endif

    bVote = false;
    ui->txtChatNick->installEventFilter(this);
    ui->txtChatMsg->installEventFilter(this);

    t_action = new QTimer(this); t_action->start(10*1000);
    connect(t_action, SIGNAL(timeout()), this, SLOT(showAction()));
    this->showAction();
}

bool OverviewPage::eventFilter(QObject *object, QEvent *event)
{
    if ((object == ui->txtChatNick || object == ui->txtChatMsg) && event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return)
        {
            this->OverviewPage::on_btnChatSend_clicked();
            return true;
        }
    }
    return QWidget::eventFilter(object, event);
}

// #talkcoin
void OverviewPage::showAction()
{
    this->showVote();
    if (ui->rbLang_en->isChecked()) this->on_rbLang_en_clicked();
    else if (ui->rbLang_ru->isChecked()) this->on_rbLang_ru_clicked();
    else if (ui->rbLang_cn->isChecked()) this->on_rbLang_cn_clicked();
}

void OverviewPage::showVote()
{
    int nHeightH = nBestHeight + (V_BlockInterval - (nBestHeight % V_BlockInterval));
    int start = nHeightH - V_BlockInit - V_Blocks;
    int end = nHeightH - V_BlockInit;

    ui->lblRewardInfo->setText("► " + tr("Current reward: ")
                               + TalkcoinUnits::formatWithUnit(TalkcoinUnits::TAC, nSubsidy)
                               + (V_Total? " (" + QString::number(V_Total) + " votes)" : "")
                              );

    if (nBestHeight < start)
    {
        int rtime = ((start - nBestHeight) / 180) + 1;

        if (rtime > 1)
            ui->lblVoteInfo->setText("► Less than " + QString::number(rtime) + tr(" hours remaining before the voting begins"));
        else
            ui->lblVoteInfo->setText("► Less than 1 hour remaining before the voting begins");
        ui->lblVoteInfo->setStyleSheet("color: black;");
        ui->spbVote->setEnabled(false);
        ui->btnVote->setEnabled(false);
    }
    else if (nBestHeight >= start && nBestHeight <= end)
    {
        ui->lblVoteInfo->setText("► " + tr("Voting is opened"));
        ui->lblVoteInfo->setStyleSheet("color: green;");
        ui->spbVote->setEnabled(bVote? false : true);
        ui->btnVote->setEnabled(bVote? false : true);
    }
    else
    {
        ui->lblVoteInfo->setText("► " + tr("Voting is closed"));
        ui->lblVoteInfo->setStyleSheet("color: red;");
        ui->spbVote->setEnabled(false);
        ui->btnVote->setEnabled(false);
        bVote = false;
    }
}

void OverviewPage::showChat()
{
    std::string tlk = "";

    for (int i = tlk_size1-1; i >= 0; i--)
    {
        if (!TLK___[i][0].empty())
        {
            std::string style = QString(TLK___[i][4].c_str()).remove(QRegExp("<[^>]*>")).toStdString();

            if (atoi(TLK___[i][1].c_str()) == GetChatBValue())
                style = "<b>" + style + "</b>";
            else if (atoi(TLK___[i][1].c_str()) == GetChatRValue())
                style = "<b><font color=red>" + style + "</font></b>";

            tlk += "<p><font color=blue>" + TLK___[i][3] + "</font> <small>("
                    + GUIUtil::dateTimeStr(atoi(TLK___[i][2].c_str())).toStdString() + ")</small><br>"
                    + style + "</p>";
        }
    }

    if (tlk != _tlk)
    {
        _tlk = tlk;
        ui->txtChat->clear();
        ui->txtChat->setHtml(tlk.c_str());
        QScrollBar *sb = ui->txtChat->verticalScrollBar();
        sb->setValue(sb->maximum());
        if (!tlk.empty() && !clientModel->inInitialBlockDownload())
            GUIUtil::playSound(":/sounds/chat");
    }
}

void OverviewPage::on_btnVote_clicked()
{
    if (ui->spbVote->value() < 1)
    {
        ui->spbVote->setStyleSheet("background-color: red;");
        return;
    }

    SendCoinsRecipient rv;
    rv.address = GetVoteAddr().c_str();
    rv.label = GetVoteLbl().c_str();
    rv.amount = GetVoteValue();
    rv.SetVote();
    rv.vote = ui->spbVote->value() * COIN;
    QList<SendCoinsRecipient> recipients;
    recipients.append(rv);

    WalletModel::SendCoinsReturn sendstatus;
    if (!walletModel->getOptionsModel() || !walletModel->getOptionsModel()->getCoinControlFeatures())
        sendstatus = walletModel->sendCoins(recipients);
    else
        sendstatus = walletModel->sendCoins(recipients, CoinControlDialog::coinControl);

    switch(sendstatus.status)
    {
    case WalletModel::AmountExceedsBalance:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("The amount to vote exceeds your balance."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::AmountWithFeeExceedsBalance:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("The total to vote exceeds your balance when the %1 transaction fee is included.").
            arg(TalkcoinUnits::formatWithUnit(TalkcoinUnits::TAC, sendstatus.fee)),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::TransactionCreationFailed:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("Vote Error: Transaction creation failed!"),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::TransactionCommitFailed:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("Vote Error: The transaction was rejected. This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::Aborted: // User aborted, nothing to do
        break;
    case WalletModel::OK:
        ui->spbVote->setEnabled(false);
        ui->btnVote->setEnabled(false);
        bVote = true;
        break;
    }
}

void OverviewPage::on_spbVote_valueChanged(int arg1)
{
    ui->spbVote->setStyleSheet("");
}

void OverviewPage::on_btnChatSend_clicked()
{
    SendCoinsRecipient rv;
    rv.address = GetChatAddr().c_str();
    rv.label = GetChatLbl().c_str();
    rv.amount = ui->btnBold->isChecked()? GetChatBValue() : GetChatValue();
    rv.SetChat();
    rv.nick = ui->txtChatNick->text().remove(QRegExp("<[^>]*>")).trimmed();
    rv.message = ui->txtChatMsg->text().remove(QRegExp("<[^>]*>")).trimmed();
    rv.data = "version=" + clientModel->formatFullVersion() + ";";
    if (ui->rbLang_en->isChecked()) rv.data += "lang=en;";
    else if (ui->rbLang_ru->isChecked()) rv.data += "lang=ru;";
    else if (ui->rbLang_cn->isChecked()) rv.data += "lang=cn;";

    bool valid = true;

    if (!ui->txtChatNick->isReadOnly())
    {
        if (rv.nick.length() < 1 || rv.nick.length() > 20
            || rv.nick.toLower() == "talkcoin"
            || rv.nick.toLower() == "administrator"
            || rv.nick.toLower() == "admin")
        {
            valid = false;
            ui->txtChatNick->setStyleSheet("background-color: red;");
        }
        else
        {
            for (unsigned int i = 0; i < tlk_size1; i++)
            {
                QString nick_en = TLK_en[i][3].c_str();
                QString nick_ru = TLK_ru[i][3].c_str();
                QString nick_cn = TLK_cn[i][3].c_str();
                if (nick_en.toLower() == rv.nick.toLower()
                    || nick_ru.toLower() == rv.nick.toLower()
                    || nick_cn.toLower() == rv.nick.toLower()
                   )
                {
                    valid = false;
                    ui->txtChatNick->setStyleSheet("background-color: red;");
                    break;
                }
            }
        }
    }

    if (rv.message.length() < 1 || rv.message.length() > 140)
    {
        valid = false;
        ui->txtChatMsg->setStyleSheet("background-color: red;");
    }

    if (valid)
    {
        QList<SendCoinsRecipient> recipients;
        recipients.append(rv);

        WalletModel::SendCoinsReturn sendstatus;
        if (!walletModel->getOptionsModel() || !walletModel->getOptionsModel()->getCoinControlFeatures())
            sendstatus = walletModel->sendCoins(recipients);
        else
            sendstatus = walletModel->sendCoins(recipients, CoinControlDialog::coinControl);

        switch(sendstatus.status)
        {
        case WalletModel::AmountExceedsBalance:
            QMessageBox::warning(this, tr("Send Coins"),
                tr("The amount to chat exceeds your balance."),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case WalletModel::AmountWithFeeExceedsBalance:
            QMessageBox::warning(this, tr("Send Coins"),
                tr("The total to chat exceeds your balance when the %1 transaction fee is included.").
                arg(TalkcoinUnits::formatWithUnit(TalkcoinUnits::TAC, sendstatus.fee)),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case WalletModel::TransactionCreationFailed:
            QMessageBox::warning(this, tr("Send Coins"),
                tr("Chat Error: Transaction creation failed!"),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case WalletModel::TransactionCommitFailed:
            QMessageBox::warning(this, tr("Send Coins"),
                tr("Chat Error: The transaction was rejected. This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here."),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case WalletModel::Aborted: // User aborted, nothing to do
            break;
        case WalletModel::OK:
            ui->txtChatNick->setReadOnly(true);
            ui->txtChatMsg->clear();
            break;
        }
    }
}

void OverviewPage::on_txtChatNick_textChanged(const QString &arg1)
{
    ui->txtChatNick->setStyleSheet("");
}

void OverviewPage::on_txtChatMsg_textChanged(const QString &arg1)
{
    ui->txtChatMsg->setStyleSheet("");
}

void OverviewPage::on_rbLang_en_clicked()
{
    if (ui->rbLang_en->isChecked())
    {
        std::copy(&TLK_en[0][0], &TLK_en[0][0]+tlk_size1*tlk_size2, &TLK___[0][0]);
        this->showChat();
    }
}
void OverviewPage::on_rbLang_ru_clicked()
{
    if (ui->rbLang_ru->isChecked())
    {
        std::copy(&TLK_ru[0][0], &TLK_ru[0][0]+tlk_size1*tlk_size2, &TLK___[0][0]);
        this->showChat();
    }
}
void OverviewPage::on_rbLang_cn_clicked()
{
    if (ui->rbLang_cn->isChecked())
    {
        std::copy(&TLK_cn[0][0], &TLK_cn[0][0]+tlk_size1*tlk_size2, &TLK___[0][0]);
        this->showChat();
    }
}

void OverviewPage::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if(model && model->getOptionsModel())
    {
        // Set up transaction list
        filter = new TransactionFilterProxy();
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->sort(TransactionTableModel::Status, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter);
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        setBalance(model->getBalance(), model->getUnconfirmedBalance(), model->getImmatureBalance());
        connect(model, SIGNAL(balanceChanged(qint64, qint64, qint64)), this, SLOT(setBalance(qint64, qint64, qint64)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
    }

    // update the display unit, to not use the default ("TAC")
    updateDisplayUnit();
}

void OverviewPage::updateDisplayUnit()
{
    if(walletModel && walletModel->getOptionsModel())
    {
        if(currentBalance != -1)
            setBalance(currentBalance, currentUnconfirmedBalance, currentImmatureBalance);

        // Update txdelegate->unit with the current unit
        txdelegate->unit = walletModel->getOptionsModel()->getDisplayUnit();

        ui->listTransactions->update();
    }
}

void OverviewPage::updateAlerts(const QString &warnings)
{
    this->ui->labelAlerts->setVisible(!warnings.isEmpty());
    this->ui->labelAlerts->setText(warnings);
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->labelWalletStatus->setVisible(fShow);
    ui->labelTransactionsStatus->setVisible(fShow);
}
