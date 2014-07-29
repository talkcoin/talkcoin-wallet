#include "overviewpage.h"
#include "ui_overviewpage.h"

#include "clientmodel.h"
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
#include "askpassphrasedialog.h"
#include "wallet.h"

#define DECORATION_SIZE 64
#define NUM_ITEMS 3

static const unsigned int tlk_size1 = sizeof(TLK_C1)/sizeof(TLK_C1[0]) - 1;
static const unsigned int tlk_size2 = sizeof(TLK_C1[0])/sizeof(TLK_C1[0][0]);

static std::string TLK___[tlk_size1][tlk_size2];
static std::string xTLK[tlk_size1];


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

    // ToolTips
    ui->rbChan1->setToolTip(TLK_CHAN[0][0].c_str());
    ui->rbChan2->setToolTip(TLK_CHAN[1][0].c_str());
    ui->btnVote->setToolTip(TalkcoinUnits::formatWithUnit(TalkcoinUnits::TAC, GET_V_VOTE()));
    ui->chkAutoVote->setToolTip(TalkcoinUnits::formatWithUnit(TalkcoinUnits::TAC, GET_V_VOTE()));
    ui->btnChatSend->setToolTip(TalkcoinUnits::formatWithUnit(TalkcoinUnits::TAC, GET_V_CHAT()));
    ui->btnChatBold->setToolTip(TalkcoinUnits::formatWithUnit(TalkcoinUnits::TAC, GET_V_CHATB()));

    // vote
    ui->spbVote->setMinimum((double)GET_V_REWARDMIN()/COIN);
    ui->spbVote->setMaximum((double)GET_V_REWARDMAX()/COIN);
    ui->spbVote->setValue((double)(GET_V_REWARDMAX()/2)/COIN);

    // chat
    ui->txtChatNick->installEventFilter(this);
    ui->txtChatMsg->installEventFilter(this);
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

    t_action = new QTimer(this); t_action->start(5*1000);
    connect(t_action, SIGNAL(timeout()), this, SLOT(showAction()));
    this->showAction();
}

// #talkcoin
void OverviewPage::showAction()
{
    if(walletModel)
        this->setWalletStatus();

    this->showVote();

    if (ui->rbChan1->isChecked()) this->on_rbChan1_clicked();
    else if (ui->rbChan2->isChecked()) this->on_rbChan2_clicked();
}

void OverviewPage::showVote()
{
    int nHeightH = nBestHeight + (V_BlockInterval - (nBestHeight % V_BlockInterval));
    int start = nHeightH - V_BlockInit - V_Blocks;
    int end = nHeightH - V_BlockInit;

    ui->lblInfo->setText(TalkcoinUnits::formatWithUnit(TalkcoinUnits::TAC, nSubsidy) + (V_Total? " (" + QString::number(V_Total) + tr(" votes)") : ""));

    if (nBestHeight < start)
    {
        int rtime = ((start - nBestHeight) / 180) + 1;
        ui->lblVoteInfo->setText("~ " + QString::number(rtime) + ((rtime>1)? tr(" hours") : tr(" hour")));
        ui->lblVoteInfo->setStyleSheet("color: white;");
        ui->btnVote->setEnabled(false);
    }
    else if (nBestHeight >= start && nBestHeight <= end)
    {
        ui->lblVoteInfo->setText(tr("Voting is open"));
        ui->lblVoteInfo->setStyleSheet("color: green;");

        if (!clientModel->inInitialBlockDownload())
        {
            if (bVote)
            {
                ui->btnVote->setEnabled(false);
            }
            else
            {
                ui->btnVote->setEnabled(true);
                if (ui->chkAutoVote->isChecked())
                {
                    ui->chkAutoVote->setChecked(Qt::Unchecked);
                    this->on_btnVote_clicked();
                }
            }
        }
    }
    else
    {
        ui->lblVoteInfo->setText(tr("Voting is closed"));
        ui->lblVoteInfo->setStyleSheet("color: red;");
        ui->btnVote->setEnabled(false);
        bVote = false;
    }
}

void OverviewPage::showChat()
{
    if (ui->rbChan2->isChecked() && TLK_CHAN[1][0] == "#")
    {
        ui->txtChat->clear();
        ui->txtChat->setHtml("<p style=\"font-size:12pt; text-align:center; color:red;\">" + tr("To create your own chat room, start talkcoin-qt with the flag <i>-chan=#yourchan</i> <i>-chanpassword=yourpassword</i> (an associated password is optional)") + "</p>");
        xTLK[0] = "#";
        return;
    }

    for (unsigned int i = 0; i < tlk_size1; i++)
    {
        if (xTLK[i] != TLK___[i][0])
        {
            for (unsigned int i = 0; i < tlk_size1; i++)
                xTLK[i] = TLK___[i][0];
            break;
        }
        if (i == tlk_size1-1 || TLK___[i][0].empty()) return;
    }


    std::string tlk = "";
    std::string nickx = "";
    CWallet *wallet;

    for (int i = tlk_size1-1; i >= 0; i--)
    {
        if (!TLK___[i][0].empty())
        {
            std::string style = QString(TLK___[i][4].c_str()).remove(QRegExp("<[^>]*>")).trimmed().toStdString();
            std::string translate = "<a href =\"https://translate.google.com/m?sl=auto&tl=auto&ie=UTF-8&prev=_m&q=" + QString(QUrl::toPercentEncoding(style.c_str())).toStdString() + "\"><img src=\":/icons/chat_translate\"></a>&nbsp;";
            style = getSmileys(style);
            std::string nick = QString(TLK___[i][3].c_str()).remove(QRegExp("<[^>]*>")).trimmed().toStdString();
            std::string encrypted = wallet->checkCrypt(TLK___[i][5], false)? "&nbsp;<img src=\":/icons/chat_encrypted\">" : "";

            if (atoi(TLK___[i][1].c_str()) == GET_V_CHATB(nBestHeight))
                style = "<b>" + style + "</b>";

            tlk += (std::string)"<table style=\"margin-bottom:15px;\"><tr><td width=\"35\">"
                   + (nickx!=nick? "<img src=\":/icons/chat_bubble\">" : "") + "</td>"
                   + "<td style=\"font-size:12pt; text-align:center; color:blue;\">" + nick + " <span style=\"font-size:small; color:#808080;\">(" + GUIUtil::dateTimeStr(atoi(TLK___[i][2].c_str())).toStdString() + ")</span>" + encrypted + "</td></tr>"
                   + "<tr><td></td><td style=\"font-size:12pt; text-align:center;\">" + translate + style + "</td></tr></table>";

            nickx = nick;
        }
    }

    ui->txtChat->clear();
    ui->txtChat->setHtml(tlk.c_str());
    if (!clientModel->inInitialBlockDownload()) GUIUtil::playSound(":/sounds/chat");
    QScrollBar *sb = ui->txtChat->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void OverviewPage::on_btnChatSmiley_clicked()
{
    QMessageBox *msgBox = new QMessageBox();
    msgBox->setWindowTitle(tr("List of emoticons"));

    msgBox->setText(
                    (QString)"<table align=\"center\" width=\"100%\"><tr><td><img src=\":/smileys/icon_smile\"></td><td><b>:)</b></td></tr>"
                    + "<tr><td><img src=\":/smileys/icon_biggrin\"></td><td><b>:D</b></td></tr>"
                    + "<tr><td><img src=\":/smileys/icon_sad\"></td><td><b>:(</b></td></tr>"
                    + "<tr><td><img src=\":/smileys/icon_surprised\"></td><td><b>:o</b></td></tr>"
                    + "<tr><td><img src=\":/smileys/icon_eek\"></td><td><b>8O</b></td></tr>"
                    + "<tr><td><img src=\":/smileys/icon_confused\"></td><td><b>:?</b></td></tr>"
                    + "<tr><td><img src=\":/smileys/icon_cool\"></td><td><b>8)</b></td></tr>"
                    + "<tr><td><img src=\":/smileys/icon_mad\"></td><td><b>:x</b></td></tr>"
                    + "<tr><td><img src=\":/smileys/icon_razz\"></td><td><b>:P</b></td></tr>"
                    + "<tr><td><img src=\":/smileys/icon_neutral\"></td><td><b>:|</b></td></tr>"
                    + "<tr><td><img src=\":/smileys/icon_wink\"></td><td><b>;)</b></td></tr></table>"
                   );

    msgBox->setStyleSheet("font-size:10pt;font-family:'Gill Sans MT'; \
                           color: white; \
                           background-color: rgb(103,47,82); \
                           alternate-background-color: rgb(89,38,68);");

    msgBox->setStandardButtons(QMessageBox::Ok);
    msgBox->setDefaultButton(QMessageBox::Ok);
    msgBox->setModal(true);
    msgBox->setWindowFlags(msgBox->windowFlags() | Qt::WindowStaysOnTopHint);
    msgBox->show();
}

std::string OverviewPage::getSmileys(std::string str)
{
    replaceAll(str, ":)", "<img src=\":/smileys/icon_smile\">");
    replaceAll(str, ":D", "<img src=\":/smileys/icon_biggrin\">");
    replaceAll(str, ":(", "<img src=\":/smileys/icon_sad\">");
    replaceAll(str, ":o", "<img src=\":/smileys/icon_surprised\">");
    replaceAll(str, "8O", "<img src=\":/smileys/icon_eek\">");
    replaceAll(str, ":?", "<img src=\":/smileys/icon_confused\">");
    replaceAll(str, "8)", "<img src=\":/smileys/icon_cool\">");
    replaceAll(str, ":x", "<img src=\":/smileys/icon_mad\">");
    replaceAll(str, ":P", "<img src=\":/smileys/icon_razz\">");
    replaceAll(str, ":|", "<img src=\":/smileys/icon_neutral\">");
    replaceAll(str, ";)", "<img src=\":/smileys/icon_wink\">");
    return str;
}

void OverviewPage::replaceAll(std::string& str, const std::string& from, const std::string& to)
{
    if(from.empty())
        return;
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}

void OverviewPage::on_btnVote_clicked()
{
    if (clientModel->inInitialBlockDownload()) return;

    if (ui->spbVote->value() < 1 && nBestHeight < HF1)
    {
        ui->spbVote->setStyleSheet("background-color: red;");
        return;
    }

    SendCoinsRecipient rv;
    rv.address = DecodeBase64(GET_A_VOTE1(nBestHeight)).c_str();
    rv.label = "Talkcoin Vote";
    rv.amount = GET_V_VOTE(nBestHeight);
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
        ui->btnVote->setEnabled(false);
        bVote = true;
        break;
    }
}

void OverviewPage::on_btnChatSend_clicked()
{
    if (clientModel->inInitialBlockDownload() || (ui->rbChan2->isChecked() && TLK_CHAN[1][0] == "#"))
        return;

    SendCoinsRecipient rv;
    rv.address = DecodeBase64(GET_A_CHAT(nBestHeight)).c_str();
    rv.label = "Talkcoin Chat";
    rv.amount = ui->btnChatBold->isChecked()? GET_V_CHATB(nBestHeight) : GET_V_CHAT(nBestHeight);
    rv.SetChat();
    rv.nick = ui->txtChatNick->text().remove(QRegExp("<[^>]*>")).trimmed();
    rv.message = ui->txtChatMsg->text().remove(QRegExp("<[^>]*>")).trimmed();
    rv.data = "version=" + clientModel->formatFullVersion() + ";";
    if (ui->rbChan1->isChecked())
        rv.data += (QString)"chan=" + TLK_CHAN[0][0].c_str() + ";";
    else if (ui->rbChan2->isChecked())
        rv.data += (QString)"chan=" + TLK_CHAN[1][0].c_str() + ";";

    bool valid = true;

    if (!ui->txtChatNick->isReadOnly())
    {
        if (rv.nick.length() < 1 || rv.nick.length() > 20 || !validNick(rv.nick.toStdString()))
        {
            valid = false;
            ui->txtChatNick->setStyleSheet("background-color: red; background-repeat: no-repeat;");
        }
        else
        {
            for (unsigned int i = 0; i < tlk_size1; i++)
            {
                QString nick_chan1 = TLK_C1[i][3].c_str();
                QString nick_chan2 = TLK_C2[i][3].c_str();
                if (nick_chan1.toLower() == rv.nick.toLower() || nick_chan2.toLower() == rv.nick.toLower())
                {
                    valid = false;
                    ui->txtChatNick->setStyleSheet("background-color: red; background-repeat: no-repeat;");
                    break;
                }
            }
        }
    }

    if (rv.message.length() < 1 || rv.message.length() > 140)
    {
        valid = false;
        ui->txtChatMsg->setStyleSheet("background-color: red; background-repeat: no-repeat;");
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
    ui->txtChatNick->setStyleSheet(" \
    background-color: rgb(255, 255, 255); \
    QToolTip { \
         color: white; \
         font-size:9pt;font-family:'Gill Sans MT'; \
         border: 4px solid; \
         border-radius: 13px; \
         opacity: 700; \
         border-color: rgb(89,38,68); \
         background-color: rgb(62,29,67); \
     }");
}

void OverviewPage::on_txtChatMsg_textChanged(const QString &arg1)
{
    ui->txtChatMsg->setStyleSheet(" \
    background-color: rgb(255, 255, 255); \
    QToolTip { \
         color: white; \
         font-size:9pt;font-family:'Gill Sans MT'; \
         border: 4px solid; \
         border-radius: 13px; \
         opacity: 700; \
         border-color: rgb(89,38,68); \
         background-color: rgb(62,29,67); \
     }");
}

void OverviewPage::on_rbChan1_clicked()
{
    std::copy(&TLK_C1[0][0], &TLK_C1[0][0]+tlk_size1*tlk_size2, &TLK___[0][0]);
    this->showChat();
}
void OverviewPage::on_rbChan2_clicked()
{
    std::copy(&TLK_C2[0][0], &TLK_C2[0][0]+tlk_size1*tlk_size2, &TLK___[0][0]);
    this->showChat();
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

void OverviewPage::on_btnWallet_clicked()
{
    WalletModel::EncryptionStatus status = walletModel->getEncryptionStatus();

    if (status == WalletModel::Locked)
    {
        AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this);
        dlg.setModel(walletModel);
        if(dlg.exec() == QDialog::Accepted)
            this->setWalletStatus();
    }
    else if (status == WalletModel::Unlocked)
    {
        walletModel->setWalletLocked(true);
        this->setWalletStatus();
    }
}

void OverviewPage::setWalletStatus()
{
    if (status != walletModel->getEncryptionStatus())
        status = walletModel->getEncryptionStatus();
    else
        return;

    switch(status)
    {
    case WalletModel::Unencrypted:
        ui->btnWallet->setIcon(QIcon(":/icons/lock_none"));
        ui->btnWallet->setToolTip(QString());
        ui->btnWallet->setEnabled(false);
        break;
    case WalletModel::Unlocked:
        ui->btnWallet->setIcon(QIcon(":/icons/lock_open"));
        ui->btnWallet->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>unlocked</b>"));
        ui->btnWallet->setEnabled(true);
        break;
    case WalletModel::Locked:
        ui->btnWallet->setIcon(QIcon(":/icons/lock_closed"));
        ui->btnWallet->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>locked</b>"));
        ui->btnWallet->setEnabled(true);
        break;
    }
}

void OverviewPage::on_btnExpand_clicked()
{
    if (ui->bottomleftframe->isVisible())
    {
        ui->bottomleftframe->setVisible(false);
        ui->toprightframe->setVisible(false);
    }
    else if (ui->topleftframe->isVisible())
    {
        ui->topleftframe->setVisible(false);
    }
    else
    {
        ui->topleftframe->setVisible(true);
        ui->bottomleftframe->setVisible(true);
        ui->toprightframe->setVisible(true);
    }
}
