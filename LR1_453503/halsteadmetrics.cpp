#include "halsteadmetrics.h"
#include <QDebug>
#include <QRegularExpression>

QSet<QString> HalsteadMetrics::keywords = {
    "as",		 "as?",	  "break",	"class",  "continue",  "do",
    "else",		 "for",	  "fun",	"if",	  "in",		   "!in",
    "interface", "is",	  "!is",	"object", "package",   "return",
    "super",	 "this",  "throw",	"try",	  "typealias", "val",
    "var",		 "when",  "while",	"main",	  "import",	   "Boolean",
    "String",	 "Int",	  "Double", "Float",  "Short",	   "Long",
    "Char",		 "repeat"};

void HalsteadMetrics::analyze(const QString& code,
                              QMap<QString, int>& operators,
                              QMap<QString, int>& operands) {
    operators.clear();
    operands.clear();

    QString cleanedCode = removeComments(code);

    // ---- Операторы ----
    // Список паттернов для операторов
    QList<QRegularExpression> operatorRegexes;
    operatorRegexes << QRegularExpression(
        "===|!==|\\+\\+|--|\\+=|-=|\\*=|/"
        "=|%=|&=|\\|=|\\^=|==|!=|<=|>=|<|(?<!-)>|=|\\+|-(?!>)|/"
        "|\\*|%|&&|\\|\\||!");
    operatorRegexes << QRegularExpression("\\bin\\b|\\b!in\\b");
    operatorRegexes << QRegularExpression("\\bis\\b|\\b!is\\b");
    operatorRegexes << QRegularExpression("\\?:");
    operatorRegexes << QRegularExpression(
        "\\breturn\\b|\\bcontinue\\b|\\bbreak\\b");
    operatorRegexes << QRegularExpression("\\bif\\b|\\bwhen\\b");
    operatorRegexes << QRegularExpression(
        "\\bdo\\b|\\bwhile\\b|\\bfor\\b|\\brepeat\\b");
    operatorRegexes << QRegularExpression(";");
    operatorRegexes << QRegularExpression("\\.\\.<|\\.\\.");
    operatorRegexes << QRegularExpression("\\[.*?\\]");
    operatorRegexes << QRegularExpression("\\{");
    operatorRegexes << QRegularExpression("\\}");
    operatorRegexes << QRegularExpression("\\(");
    operatorRegexes << QRegularExpression("\\)");

    // Вызовы функций (исключая ключевые слова)
    QRegularExpression callRegex("\\b([a-zA-Z_]\\w*)\\s*\\(");


    for (const QRegularExpression& re : operatorRegexes) {
        QRegularExpressionMatchIterator it = re.globalMatch(cleanedCode);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            QString op = match.captured();
            operators[op] = operators.value(op, 0) + 1;
        }
    }

    // Отдельно ищем вызовы функций, проверяя, что имя не ключевое слово
    QRegularExpressionMatchIterator callIt = callRegex.globalMatch(cleanedCode);
    while (callIt.hasNext()) {
        QRegularExpressionMatch match = callIt.next();
        QString funcName = match.captured(1);
        if (!keywords.contains(funcName)) {
            operators[funcName + "()"] =
                operators.value(funcName + "()", 0) + 1;
        }
    }

    // ---- Операнды ----
    // Идентификаторы (исключая ключевые слова)
    QRegularExpression idRegex("\\b([a-zA-Z_]\\w*)\\b");
    QRegularExpressionMatchIterator idIt = idRegex.globalMatch(cleanedCode);
    while (idIt.hasNext()) {
        QRegularExpressionMatch match = idIt.next();
        QString ident = match.captured(1);
        if (!keywords.contains(ident)) {
            operands[ident] = operands.value(ident, 0) + 1;
        }
    }

    // Строковые литералы
    QRegularExpression stringRegex("\"([^\"\\\\]*(\\\\.[^\"\\\\]*)*)\"");
    QRegularExpressionMatchIterator strIt =
        stringRegex.globalMatch(cleanedCode);
    while (strIt.hasNext()) {
        QRegularExpressionMatch match = strIt.next();
        operands[match.captured()] = operands.value(match.captured(), 0) + 1;
    }

    // Символьные литералы
    QRegularExpression charRegex("'([^'\\\\]*(\\\\.[^'\\\\]*)*)'");
    QRegularExpressionMatchIterator charIt = charRegex.globalMatch(cleanedCode);
    while (charIt.hasNext()) {
        QRegularExpressionMatch match = charIt.next();
        operands[match.captured()] = operands.value(match.captured(), 0) + 1;
    }

    // Числовые литералы (целые и с плавающей точкой)
    QRegularExpression numRegex("\\b\\d+(\\.\\d+)?([eE][+-]?\\d+)?\\b");
    QRegularExpressionMatchIterator numIt = numRegex.globalMatch(cleanedCode);
    while (numIt.hasNext()) {
        QRegularExpressionMatch match = numIt.next();
        operands[match.captured()] = operands.value(match.captured(), 0) + 1;
    }
}

QString HalsteadMetrics::removeComments(const QString& code) {
    QString result = code;
    // Удаляем однострочные комментарии
    QRegularExpression singleLineComment("//[^\n]*");
    result.remove(singleLineComment);

    // Удаляем многострочные комментарии
    QRegularExpression multiLineComment(
        "/\\*.*?\\*/", QRegularExpression::DotMatchesEverythingOption);
    result.remove(multiLineComment);

    return result;
}
