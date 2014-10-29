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
#include <QScrollBar>
#include <QtWebKit>
#include <QClipboard>
#include "coincontroldialog.h"
#include "util.h"
#include "base58.h"
#include "askpassphrasedialog.h"
#include "wallet.h"
#include <boost/algorithm/string/join.hpp>

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

    // ToolTip
    ui->rbChan1->setToolTip(TLK_CHAN[0][0].c_str());
    ui->rbChan2->setToolTip(TLK_CHAN[1][0].c_str());
    ui->btnChatSend->setToolTip(TalkcoinUnits::formatWithUnit(TalkcoinUnits::TAC, GET_V_CHAT()));
    ui->btnChatBold->setToolTip(TalkcoinUnits::formatWithUnit(TalkcoinUnits::TAC, GET_V_CHATB()));
    ui->btnChatVideo->setToolTip(TalkcoinUnits::formatWithUnit(TalkcoinUnits::TAC, GET_V_CHATV()));

    // Vote
    /*ui->btnVote->setToolTip(TalkcoinUnits::formatWithUnit(TalkcoinUnits::TAC, GET_V_VOTE()));
    ui->chkAutoVote->setToolTip(TalkcoinUnits::formatWithUnit(TalkcoinUnits::TAC, GET_V_VOTE()));
    ui->spbVote->setMinimum((double)GET_V_REWARDMIN()/COIN);
    ui->spbVote->setMaximum((double)GET_V_REWARDMAX()/COIN);
    ui->spbVote->setValue((double)(GET_V_REWARDMAX()/2)/COIN);*/

    // Chat
    ui->txtChatNick->installEventFilter(this);
    ui->txtChatMsg->installEventFilter(this);
    ui->txtChat->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->txtChat, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showContextMenu(const QPoint &)));
    ui->txtChat->verticalScrollBar()->setContextMenuPolicy(Qt::NoContextMenu);

    // webView
    ui->btnHide->setVisible(false);
    ui->webView->setVisible(false);
    QNetworkProxyFactory::setUseSystemConfiguration(true);
    QWebSettings::globalSettings()->setAttribute(QWebSettings::PluginsEnabled, true);
    QWebSettings::globalSettings()->setAttribute(QWebSettings::JavascriptEnabled, true );
    QWebSettings::globalSettings()->setAttribute(QWebSettings::AutoLoadImages, true);
    QWebSettings::globalSettings()->setAttribute(QWebSettings::PrivateBrowsingEnabled, false);
    QWebSettings::globalSettings()->setAttribute(QWebSettings::DnsPrefetchEnabled, true);
    QWebSettings::globalSettings()->setAttribute(QWebSettings::LocalStorageDatabaseEnabled, true);
    ui->webView->page()->mainFrame()->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);
    ui->webView->page()->mainFrame()->setScrollBarPolicy(Qt::Vertical, Qt::ScrollBarAlwaysOff);
    ui->webView->setContextMenuPolicy(Qt::NoContextMenu);

    //QNetworkProxy proxy;
    //proxy.setType(QNetworkProxy::HttpProxy);
    //proxy.setHostName("ip");
    //proxy.setPassword("pwd");
    //proxy.setPort(8008);
    //QNetworkProxy::setApplicationProxy(proxy);
}

bool OverviewPage::eventFilter(QObject *object, QEvent *event)
{
    if ((object == ui->txtChatNick || object == ui->txtChatMsg) && event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
        {
            this->OverviewPage::on_btnChatSend_clicked();
            return true;
        }
    }

    return QWidget::eventFilter(object, event);
}

void OverviewPage::showContextMenu(const QPoint &pt)
{
    QAction *copyText = new QAction(tr("Copy"), this);
    copyText->setShortcut(QKeySequence::Copy);
    QTextCursor cursor(ui->txtChat->textCursor()); if (!cursor.hasSelection()) copyText->setEnabled(false);
    connect(copyText, SIGNAL(triggered()), this, SLOT(onCopyText()));
    QMenu *menu = new QMenu(this);
    menu->addAction(copyText);
    menu->exec(ui->txtChat->mapToGlobal(pt));
    delete copyText, menu;
}

void OverviewPage::onCopyText()
{
    QTextCursor cursor(ui->txtChat->textCursor());
    if (cursor.hasSelection())
        QApplication::clipboard()->setText(cursor.selectedText());
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

    //bVote = false;
    bChat = true;

    t_action = new QTimer(this); t_action->start(5*1000);
    connect(t_action, SIGNAL(timeout()), this, SLOT(showAction()));
    this->showAction();
}

// DEV
void delay(int secondsToWait)
{
    QTime dieTime = QTime::currentTime().addSecs(secondsToWait);
    while(QTime::currentTime() < dieTime)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
}

// #talkcoin
void OverviewPage::showAction()
{
    if(walletModel)
        this->setWalletStatus();

    //this->showVote();

    if (ui->rbChan1->isChecked()) this->on_rbChan1_clicked();
    else if (ui->rbChan2->isChecked()) this->on_rbChan2_clicked();
}

/*void OverviewPage::showVote()
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
                    // DEV
                    //for (unsigned int i = 0; i < 10; i++)
                    //{
                        //delay(1);
                        this->on_btnVote_clicked();
                    //}

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
}*/

void OverviewPage::showChat()
{
    if (ui->rbChan2->isChecked() && TLK_CHAN[1][0] == "#")
    {
        QString html;
        std::string xchan = boost::algorithm::join(XCHAN, " ");
        html = "<p style=\"font-size:12pt; text-align:center; color:red;\">" + tr("To join or create a chat room, start talkcoin-qt with the flag <i>-chan=#yourchan</i> <i>-chanpassword=yourpassword</i> (an associated password is optional)") + "</p>";
        if (!xchan.empty())
        {
            html += "<p style=\"font-size:12pt; text-align:center; color:green;\">" + tr("Public room(s) available: ") + xchan.c_str() + "</p>";
        }

        ui->txtChat->clear();
        ui->txtChat->setHtml(html);
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


    tlkchat = "";
    std::string lastnick = "";
    std::string msgv = "", hash = "";
    CWallet *wallet;

    for (int i = tlk_size1-1; i >= 0; i--)
    {
        if (!TLK___[i][0].empty())
        {
            std::string nick = stripTags(TLK___[i][3]);
            std::string msg, msgvret;
            msg = msgvret = stripTags(TLK___[i][4]);
            std::string firsticon = "<a href =\"https://translate.google.com/m?sl=auto&tl=auto&ie=UTF-8&prev=_m&q=" + QString(QUrl::toPercentEncoding(msg.c_str())).toStdString() + "\"><img src=\":/icons/chat_translate\"></a>&nbsp;";
            std::string encrypted = wallet->checkCrypt(TLK___[i][5])? "&nbsp;<img src=\":/icons/chat_encrypted\">" : "";

            if (atoi(TLK___[i][1].c_str()) == GET_V_CHATV() && getVid(msgvret))
            {
                firsticon = "<a href =\"@" + msg + "\"><img src=\":/icons/video_play\"></a>";
                msgv = msgvret;
                msg = "";

                if (std::find(PlayList.begin(), PlayList.end(), TLK___[i][0]) != PlayList.end())
                    msgv = "";
                else
                    hash = TLK___[i][0];
            }
            else
            {
                msg = getSmileys(msg);
                if (atoi(TLK___[i][1].c_str()) == GET_V_CHATB())
                    msg = "<b>" + msg + "</b>";
            }

            tlkchat += (std::string)"<table style=\"margin-bottom:10px;\"><tr><td width=\"35\">"
                   + (nick!=lastnick? "<img src=\":/icons/chat_bubble\">" : "") + "</td>"
                   + "<td style=\"font-size:12pt; color:blue;\">"
                   + (validNick(nick)? nick : "<span style=\"font-weight:bold; color:red;\">" + nick + "</span>")
                   + "&nbsp;<span style=\"font-size:small; color:#808080;\">(" + GUIUtil::dateTimeStr(atoi(TLK___[i][2].c_str())).toStdString() + ")</span>" + encrypted + "</td></tr>"
                   + "<tr><td></td><td style=\"font-size:12pt;\">" + firsticon + msg + "</td></tr></table>";

            lastnick = nick;
        }
    }

    ui->txtChat->clear();
    ui->txtChat->setHtml(tlkchat.c_str());
    if (!clientModel->inInitialBlockDownload()) GUIUtil::playSound(":/sounds/chat");
    QScrollBar *sb = ui->txtChat->verticalScrollBar();
    sb->setValue(sb->maximum());

    if (!msgv.empty() && !ui->btnHide->isVisible())
    {
        PlayList.push_back(hash);
        ui->btnHide->setVisible(true);
        ui->webView->setVisible(true);
        ui->webView->setHtml(msgv.c_str());
    }

    if (ui->txtChatNick->isReadOnly() && TLK___[0][3] == stripTags(ui->txtChatNick->text().toStdString()) && (TLK___[1][3] == TLK___[0][3] && TLK___[2][3] == TLK___[0][3] && TLK___[3][3] == TLK___[0][3] && TLK___[4][3] == TLK___[0][3]))
        bChat = false;
    else
        bChat = true;
}

void OverviewPage::on_btnChatSmiley_clicked()
{
    QMessageBox *msgBox = new QMessageBox();
    msgBox->setWindowTitle(tr("List of emoticons"));

    msgBox->setText(
                    (QString)"<table align=\"center\" width=\"100%\">"
                    + "<tr><td style=\"vertical-align:middle;\"><img src=\":/smiley/icon_smile\"></td><td style=\"vertical-align:middle;\"><b>:)</b></td></tr>"
                    + "<tr><td style=\"vertical-align:middle;\"><img src=\":/smiley/icon_biggrin\"></td><td style=\"vertical-align:middle;\"><b>:D</b></td></tr>"
                    + "<tr><td style=\"vertical-align:middle;\"><img src=\":/smiley/icon_sad\"></td><td style=\"vertical-align:middle;\"><b>:(</b></td></tr>"
                    + "<tr><td style=\"vertical-align:middle;\"><img src=\":/smiley/icon_surprised\"></td><td style=\"vertical-align:middle;\"><b>:o</b></td></tr>"
                    + "<tr><td style=\"vertical-align:middle;\"><img src=\":/smiley/icon_eek\"></td><td style=\"vertical-align:middle;\"><b>8O</b></td></tr>"
                    + "<tr><td style=\"vertical-align:middle;\"><img src=\":/smiley/icon_confused\"></td><td style=\"vertical-align:middle;\"><b>:?</b></td></tr>"
                    + "<tr><td style=\"vertical-align:middle;\"><img src=\":/smiley/icon_cool\"></td><td style=\"vertical-align:middle;\"><b>8)</b></td></tr>"
                    + "<tr><td style=\"vertical-align:middle;\"><img src=\":/smiley/icon_mad\"></td><td style=\"vertical-align:middle;\"><b>:x</b></td></tr>"
                    + "<tr><td style=\"vertical-align:middle;\"><img src=\":/smiley/icon_razz\"></td><td style=\"vertical-align:middle;\"><b>:P</b></td></tr>"
                    + "<tr><td style=\"vertical-align:middle;\"><img src=\":/smiley/icon_neutral\"></td><td style=\"vertical-align:middle;\"><b>:|</b></td></tr>"
                    + "<tr><td style=\"vertical-align:middle;\"><img src=\":/smiley/icon_wink\"></td><td style=\"vertical-align:middle;\"><b>;)</b></td></tr>"
                    + "<tr><td style=\"vertical-align:middle;\"><img src=\":/smiley/icon_troll\"></td><td style=\"vertical-align:middle;\"><b>:troll:</b></td></tr>"
                    + "</table>"
                   );

    msgBox->setStyleSheet("font-size:10pt;font-family:'Lato'; \
                           color: white; \
                           background-color: rgb(38, 61, 121); \
                           alternate-background-color: rgb(30, 51, 105);");

    msgBox->setStandardButtons(QMessageBox::Ok);
    msgBox->setDefaultButton(QMessageBox::Ok);
    msgBox->setModal(true);
    msgBox->setWindowFlags(msgBox->windowFlags() | Qt::WindowStaysOnTopHint);
    msgBox->show();
}

std::string OverviewPage::getSmileys(std::string str)
{
    replaceAll(str, ":)", "<img src=\":/smiley/icon_smile\">");
    replaceAll(str, ":D", "<img src=\":/smiley/icon_biggrin\">");
    replaceAll(str, ":(", "<img src=\":/smiley/icon_sad\">");
    replaceAll(str, ":o", "<img src=\":/smiley/icon_surprised\">");
    replaceAll(str, "8O", "<img src=\":/smiley/icon_eek\">");
    replaceAll(str, ":?", "<img src=\":/smiley/icon_confused\">");
    replaceAll(str, "8)", "<img src=\":/smiley/icon_cool\">");
    replaceAll(str, ":x", "<img src=\":/smiley/icon_mad\">");
    replaceAll(str, ":P", "<img src=\":/smiley/icon_razz\">");
    replaceAll(str, ":|", "<img src=\":/smiley/icon_neutral\">");
    replaceAll(str, ";)", "<img src=\":/smiley/icon_wink\">");
    replaceAll(str, ":troll:", "<img src=\":/smiley/icon_troll\">");
    return str;
}

Embed e_youtube = (std::string)"aHR0cDovL3d3dy55b3V0dWJlLmNvbS93YXRjaD92PXwmfDxpZnJhbWUgd2lkdGg9IjQ4MCIgaGVpZ2h0PSIyNzAiIHNyYz0iaHR0cDovL3d3dy55b3V0dWJlLmNvbS9lbWJlZC8kSUQvP2F1dG9wbGF5PTEiIGZyYW1lYm9yZGVyPSIwIiBhbGxvd2Z1bGxzY3JlZW4+PC9pZnJhbWU+";
Embed e_youku = (std::string)"aHR0cDovL3YueW91a3UuY29tL3Zfc2hvdy9pZF98Lnw8ZW1iZWQgc3JjPSJodHRwOi8vcGxheWVyLnlvdWt1LmNvbS9wbGF5ZXIucGhwL3NpZC8kSUQvdi5zd2Y/aXNBdXRvUGxheT10cnVlIiB0eXBlPSJhcHBsaWNhdGlvbi94LXNob2Nrd2F2ZS1mbGFzaCIgYWxsb3dGdWxsU2NyZWVuPSJ0cnVlIiB3aWR0aD0iNDgwIiBoZWlnaHQ9IjI3MCI+PC9lbWJlZD4=";
Embed e_tudou = (std::string)"aHR0cDovL3d3dy50dWRvdS5jb20vfC58PGVtYmVkIHNyYz0iaHR0cDovL3d3dy50dWRvdS5jb20vdi8kSUQvJmF1dG9QbGF5PXRydWUvdi5zd2YiIHR5cGU9ImFwcGxpY2F0aW9uL3gtc2hvY2t3YXZlLWZsYXNoIiBhbGxvd3NjcmlwdGFjY2Vzcz0iYWx3YXlzIiBhbGxvd2Z1bGxzY3JlZW49InRydWUiIHdpZHRoPSI0ODAiIGhlaWdodD0iMjcwIj48L2VtYmVkPg==";
Embed e_dailymotion = (std::string)"aHR0cDovL3d3dy5kYWlseW1vdGlvbi5jb20vdmlkZW8vfF98PGlmcmFtZSBmcmFtZWJvcmRlcj0iMCIgd2lkdGg9IjQ4MCIgaGVpZ2h0PSIyNzAiIHNyYz0iaHR0cDovL3d3dy5kYWlseW1vdGlvbi5jb20vZW1iZWQvdmlkZW8vJElEP2F1dG9QbGF5PTEiIGFsbG93ZnVsbHNjcmVlbj48L2lmcmFtZT4=";
Embed e_rutube = (std::string)"aHR0cDovL3J1dHViZS5ydS92aWRlby98L3w8aWZyYW1lIHdpZHRoPSI0ODAiIGhlaWdodD0iMjcwIiBzcmM9Imh0dHA6Ly9ydXR1YmUucnUvcGxheS9lbWJlZC8kSUQiIGZyYW1lYm9yZGVyPSIwIiB3ZWJraXRBbGxvd0Z1bGxTY3JlZW4gbW96YWxsb3dmdWxsc2NyZWVuIGFsbG93ZnVsbHNjcmVlbj48L2lmcmFtZT4=";

bool OverviewPage::getVid(std::string &ret)
{
    QString link = QString(ret.c_str()).replace("https://", "http://").replace("youtu.be", "youtube.com");

    if (e_youtube.ValidLink(link.toStdString()))
    {
        ret = e_youtube.ReplaceID(link.section(e_youtube.GetLink().c_str(), 1).section(e_youtube.GetSeparator().c_str(), 0, 0).toStdString());
        return true;
    }
    else if (e_youku.ValidLink(link.toStdString()))
    {
        ret = e_youku.ReplaceID(link.section(e_youku.GetLink().c_str(), 1).section(e_youku.GetSeparator().c_str(), 0, 0).toStdString());
        return true;
    }
    else if (e_tudou.ValidLink(link.toStdString()))
    {
        QStringList list = link.split("/");
        ret = e_tudou.ReplaceID(list[list.at(list.count()-1).isEmpty()? list.count()-2 : list.count()-1].section(e_tudou.GetSeparator().c_str(), 0, 0).toStdString());
        return true;
    }
    else if (e_dailymotion.ValidLink(link.toStdString()))
    {
        ret = e_dailymotion.ReplaceID(link.section(e_dailymotion.GetLink().c_str(), 1).section(e_dailymotion.GetSeparator().c_str(), 0, 0).toStdString());
        return true;
    }
    else if (e_rutube.ValidLink(link.toStdString()))
    {
        ret = e_rutube.ReplaceID(link.section(e_rutube.GetLink().c_str(), 1).section(e_rutube.GetSeparator().c_str(), 0, 0).toStdString());
        return true;
    }
    else
        return false;
}

std::string OverviewPage::stripTags(std::string str)
{
    QString qstring = str.c_str();
    return qstring.remove(QRegExp("<[^>]*>")).trimmed().toStdString();
}

/*void OverviewPage::on_btnVote_clicked()
{
    if (clientModel->inInitialBlockDownload()) return;

    SendCoinsRecipient rv;
    rv.address = DecodeBase64(GET_A_VOTE1()).c_str();
    rv.label = "Talkcoin (genesis block address)";
    rv.amount = GET_V_VOTE();
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
}*/

void OverviewPage::on_btnChatSend_clicked()
{
    if (clientModel->inInitialBlockDownload() || (ui->rbChan2->isChecked() && TLK_CHAN[1][0] == "#"))
        return;

    SendCoinsRecipient rv;
    rv.address = DecodeBase64(GET_A_CHAT()).c_str();
    rv.label = "Talkcoin (genesis block address)";
    rv.amount = ui->btnChatBold->isChecked()? GET_V_CHATB() :
                                              ui->btnChatVideo->isChecked()? GET_V_CHATV() : GET_V_CHAT();
    rv.SetChat();
    rv.nick = stripTags(ui->txtChatNick->text().toStdString()).c_str();
    rv.message = stripTags(ui->txtChatMsg->text().toStdString()).c_str();
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

    if (valid && bChat)
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
            ui->btnChatBold->setChecked(false);
            ui->btnChatVideo->setChecked(false);
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
         font-size:9pt;font-family:'Lato'; \
         border: 4px solid; \
         border-radius: 13px; \
         opacity: 700; \
         border-color: rgb(30, 51, 105); \
         background-color: rgb(23, 39, 79); \
     }");
}

void OverviewPage::on_txtChatMsg_textChanged(const QString &arg1)
{
    ui->txtChatMsg->setStyleSheet(" \
    background-color: rgb(255, 255, 255); \
    QToolTip { \
         color: white; \
         font-size:9pt;font-family:'Lato'; \
         border: 4px solid; \
         border-radius: 13px; \
         opacity: 700; \
         border-color: rgb(30, 51, 105); \
         background-color: rgb(23, 39, 79); \
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
    ui->topleftframe->setVisible(!ui->topleftframe->isVisible());
    ui->toprightframe->setVisible(!ui->toprightframe->isVisible());
}

void OverviewPage::on_btnHide_clicked()
{
    ui->btnHide->setVisible(false);
    ui->webView->setVisible(false);
    ui->webView->setUrl(QUrl("about:blank"));
}

void OverviewPage::on_txtChat_anchorClicked(const QUrl &arg1)
{
    std::string msgv = QString(arg1.toString()).mid(1).toStdString();
    if (getVid(msgv))
    {
        ui->btnHide->setVisible(true);
        ui->webView->setVisible(true);
        ui->webView->setHtml(msgv.c_str());
    }

    QScrollBar *sb = ui->txtChat->verticalScrollBar();
    const int sbval = sb->value();
    ui->txtChat->clear();
    ui->txtChat->setHtml(tlkchat.c_str());
    sb->setValue(sbval);
}
