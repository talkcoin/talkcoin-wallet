#include "uritests.h"
#include "../guiutil.h"
#include "../walletmodel.h"

#include <QUrl>

void URITests::uriTests()
{
    SendCoinsRecipient rv;
    QUrl uri;
    uri.setUrl(QString("talkcoin:TityLcNJr19fC3GREkNbyi5YFpN26iLjEZ?req-dontexist="));
    QVERIFY(!GUIUtil::parseTalkcoinURI(uri, &rv));

    uri.setUrl(QString("talkcoin:TityLcNJr19fC3GREkNbyi5YFpN26iLjEZ?dontexist="));
    QVERIFY(GUIUtil::parseTalkcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("TityLcNJr19fC3GREkNbyi5YFpN26iLjEZ"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("talkcoin:TityLcNJr19fC3GREkNbyi5YFpN26iLjEZ?label=Wikipedia Example Address"));
    QVERIFY(GUIUtil::parseTalkcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("TityLcNJr19fC3GREkNbyi5YFpN26iLjEZ"));
    QVERIFY(rv.label == QString("Wikipedia Example Address"));
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("talkcoin:TityLcNJr19fC3GREkNbyi5YFpN26iLjEZ?amount=0.001"));
    QVERIFY(GUIUtil::parseTalkcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("TityLcNJr19fC3GREkNbyi5YFpN26iLjEZ"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100000);

    uri.setUrl(QString("talkcoin:TityLcNJr19fC3GREkNbyi5YFpN26iLjEZ?amount=1.001"));
    QVERIFY(GUIUtil::parseTalkcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("TityLcNJr19fC3GREkNbyi5YFpN26iLjEZ"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100100000);

    uri.setUrl(QString("talkcoin:TityLcNJr19fC3GREkNbyi5YFpN26iLjEZ?amount=100&label=Wikipedia Example"));
    QVERIFY(GUIUtil::parseTalkcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("TityLcNJr19fC3GREkNbyi5YFpN26iLjEZ"));
    QVERIFY(rv.amount == 10000000000LL);
    QVERIFY(rv.label == QString("Wikipedia Example"));

    uri.setUrl(QString("talkcoin:TityLcNJr19fC3GREkNbyi5YFpN26iLjEZ?message=Wikipedia Example Address"));
    QVERIFY(GUIUtil::parseTalkcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("TityLcNJr19fC3GREkNbyi5YFpN26iLjEZ"));
    QVERIFY(rv.label == QString());

    QVERIFY(GUIUtil::parseTalkcoinURI("talkcoin://TityLcNJr19fC3GREkNbyi5YFpN26iLjEZ?message=Wikipedia Example Address", &rv));
    QVERIFY(rv.address == QString("TityLcNJr19fC3GREkNbyi5YFpN26iLjEZ"));
    QVERIFY(rv.label == QString());

    // We currently don't implement the message parameter (ok, yea, we break spec...)
    uri.setUrl(QString("talkcoin:TityLcNJr19fC3GREkNbyi5YFpN26iLjEZ?req-message=Wikipedia Example Address"));
    QVERIFY(!GUIUtil::parseTalkcoinURI(uri, &rv));

    uri.setUrl(QString("talkcoin:TityLcNJr19fC3GREkNbyi5YFpN26iLjEZ?amount=1,000&label=Wikipedia Example"));
    QVERIFY(!GUIUtil::parseTalkcoinURI(uri, &rv));

    uri.setUrl(QString("talkcoin:TityLcNJr19fC3GREkNbyi5YFpN26iLjEZ?amount=1,000.0&label=Wikipedia Example"));
    QVERIFY(!GUIUtil::parseTalkcoinURI(uri, &rv));
}
