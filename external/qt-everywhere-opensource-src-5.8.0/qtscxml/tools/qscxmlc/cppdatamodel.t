QString ${datamodel}::evaluateToString(QScxmlExecutableContent::EvaluatorId id, bool *ok)
{
    *ok = true;
${evaluateToStringCases}
    Q_UNREACHABLE();
    *ok = false;
    return QString();
}

bool ${datamodel}::evaluateToBool(QScxmlExecutableContent::EvaluatorId id, bool *ok)
{
    *ok = true;
${evaluateToBoolCases}
    Q_UNREACHABLE();
    *ok = false;
    return false;
}

QVariant ${datamodel}::evaluateToVariant(QScxmlExecutableContent::EvaluatorId id, bool *ok)
{
    *ok = true;
${evaluateToVariantCases}
    Q_UNREACHABLE();
    *ok = false;
    return QVariant();
}

void ${datamodel}::evaluateToVoid(QScxmlExecutableContent::EvaluatorId id, bool *ok)
{
    *ok = true;
${evaluateToVoidCases}
    Q_UNREACHABLE();
    *ok = false;
}
