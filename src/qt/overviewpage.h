#ifndef OVERVIEWPAGE_H
#define OVERVIEWPAGE_H

#include "walletmodel.h"
#include <QWidget>

#include <QUrl>
#include "base58.h"
#include <boost/algorithm/string.hpp>

namespace Ui {
class OverviewPage;
}
class ClientModel;
class WalletModel;
class TxViewDelegate;
class TransactionFilterProxy;

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Overview ("home") page widget */
class OverviewPage : public QWidget
{
    Q_OBJECT

public:
    explicit OverviewPage(QWidget *parent = 0);
    ~OverviewPage();

    void setClientModel(ClientModel *clientModel);
    void setWalletModel(WalletModel *walletModel);
    void showOutOfSyncWarning(bool fShow);

protected:
    bool eventFilter(QObject *object, QEvent *event);

public slots:
    void setBalance(qint64 balance, qint64 unconfirmedBalance, qint64 immatureBalance);

signals:
    void transactionClicked(const QModelIndex &index);

private:
    Ui::OverviewPage *ui;
    ClientModel *clientModel;
    WalletModel *walletModel;
    qint64 currentBalance;
    qint64 currentUnconfirmedBalance;
    qint64 currentImmatureBalance;

    TxViewDelegate *txdelegate;
    TransactionFilterProxy *filter;

    QTimer *t_action;
    //bool bVote;
    bool bChat;
    std::string tlkchat;
    std::vector<std::string> PlayList;
    WalletModel::EncryptionStatus status;

private slots:
    void updateDisplayUnit();
    void handleTransactionClicked(const QModelIndex &index);
    void updateAlerts(const QString &warnings);
    void showContextMenu(const QPoint &pt);
    void onCopyText();
    void showAction();
    //void showVote();
    void showChat();
    std::string getSmileys(std::string str);
    bool getVid(std::string &ret);
    std::string stripTags(std::string str);

    //void on_btnVote_clicked();
    void on_btnChatSmiley_clicked();
    void on_btnChatSend_clicked();
    void on_txtChatNick_textChanged(const QString &arg1);
    void on_txtChatMsg_textChanged(const QString &arg1);
    void on_rbChan1_clicked();
    void on_rbChan2_clicked();
    void on_btnWallet_clicked();
    void setWalletStatus();
    void on_btnExpand_clicked();
    void on_btnHide_clicked();
    void on_txtChat_anchorClicked(const QUrl &arg1);
};

class Embed
{
public:
    std::vector<std::string> strs;

    const std::string GetLink()
    {
        return strs.at(0);
    }

    const std::string GetSeparator()
    {
        return strs.at(1);
    }

    const bool ValidLink(const std::string link)
    {
        return (link.find(" ") == std::string::npos && link.substr(0, this->GetLink().length()) == this->GetLink());
    }

    const std::string ReplaceID(std::string ID)
    {
        std::string player = strs.at(2);
        replaceAll(player, "$ID", ID);
        return player;
    }

    Embed()
    {
    }

    Embed(std::string xdata)
    {
        xdata = DecodeBase64(xdata);
        boost::split(strs, xdata, boost::is_any_of("|"));
    }
};

#endif // OVERVIEWPAGE_H
