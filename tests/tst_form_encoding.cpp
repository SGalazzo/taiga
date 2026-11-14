#include <QTest>
#include <QUrlQuery>

#include "taiga/network.hpp"

using namespace Qt::Literals::StringLiterals;

// application/x-www-form-urlencoded decodes '+' as a space, so a literal '+' in a value
// must be sent as %2B, and a literal '%' as %25. Decode the body the way a form server would
// and compare.
namespace {

QString decodeFormValue(const QByteArray& body, const QString& key) {
  QString text = QString::fromUtf8(body);
  text.replace(u'+', u' ');  // form semantics
  return QUrlQuery{text}.queryItemValue(key, QUrl::FullyDecoded);
}

void checkRoundTrip(const QString& key, const QString& value) {
  const auto encoded = taiga::formUrlEncode({{key, value}});
  QCOMPARE(decodeFormValue(encoded, key), value);
}

}  // namespace

class TestFormEncoding : public QObject {
  Q_OBJECT

private slots:
  void passwordWithPlus() { checkRoundTrip(u"password"_s, u"a+b c&d=e"_s); }
  void commentsWithPlus() { checkRoundTrip(u"comments"_s, u"C++ is 1+1"_s); }
  void percentSequenceKeptLiteral() { checkRoundTrip(u"comments"_s, u"100%41 done %"_s); }
};

QTEST_GUILESS_MAIN(TestFormEncoding)
#include "tst_form_encoding.moc"
