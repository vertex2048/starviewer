#include "StarviewerApplicationCommandLine.h"

#include "applicationcommandlineoptions.h"
#include "logging.h"

namespace udg
{

const QString StarviewerApplicationCommandLine::accessionNumberOption("accessionnumber");

ApplicationCommandLineOptions StarviewerApplicationCommandLine::getStarviewerApplicationCommandLineOptions()
{
    ApplicationCommandLineOptions starviewerCommandLineOptions;

    #ifndef STARVIEWER_LITE //Opció no disponible Starviewer Lite
    starviewerCommandLineOptions.addOption(accessionNumberOption, QObject::tr("Query the study with this accession number in the configurated default pacs and retrieve it"));
    #endif

    return starviewerCommandLineOptions;
}

bool StarviewerApplicationCommandLine::parse(QStringList argumentsList, QString &errorInvalidCommanLineArguments)
{
    ApplicationCommandLineOptions commandLineOptions = getStarviewerApplicationCommandLineOptions();
    bool ok = commandLineOptions.parseArgumentList(argumentsList);

    if (ok)
    {
        errorInvalidCommanLineArguments = "";
    }
    else
    {
        errorInvalidCommanLineArguments = commandLineOptions.getParserErrorMessage();
    }

    return ok;
}

bool StarviewerApplicationCommandLine::parseAndRun(const QString &argumentsListAsQString, QString &errorInvalidCommanLineArguments)
{
    return parseAndRun(argumentsListAsQString.split(";"), errorInvalidCommanLineArguments);
}

bool StarviewerApplicationCommandLine::parseAndRun(const QString &argumentsListAsQString)
{
    QString unusedQString;

    return parseAndRun(argumentsListAsQString, unusedQString);
}

bool StarviewerApplicationCommandLine::parseAndRun(QStringList arguments, QString &errorInvalidCommanLineArguments)
{
    ApplicationCommandLineOptions commandLineOptions = getStarviewerApplicationCommandLineOptions();

    if (commandLineOptions.parseArgumentList(arguments))
    {
        if (commandLineOptions.getNumberOfParsedOptions() == 0)
        {
            /*Vol dir que han executat una nova instància del Starviewer que ha detectat que hi havia una altra instància executant-se
              i ens ha enviat un missatge en blanc perquè obrim un nova finestra d'Starviewer*/
            QPair<StarviewerCommandLineOption, QString> commandLineOptionValue(openBlankWindow, "");
            AddOptionToCommandLineOptionListToProcess(commandLineOptionValue);
        }
        else
        {
            if (commandLineOptions.isSet(accessionNumberOption))
            {
                QPair<StarviewerCommandLineOption, QString> commandLineOptionValue(retrieveStudyFromAccessioNumber, commandLineOptions.getOptionArgument(accessionNumberOption));
                AddOptionToCommandLineOptionListToProcess(commandLineOptionValue);
            }
        }

        emit newOptionsToRun();
        errorInvalidCommanLineArguments = "";

        return true;
    }
    else
    {
        errorInvalidCommanLineArguments = commandLineOptions.getParserErrorMessage();
        return false;
    }
}

bool StarviewerApplicationCommandLine::takeOptionToRun(QPair<StarviewerApplicationCommandLine::StarviewerCommandLineOption,QString> &optionValue)
{   
    bool optionValueTaken = false;
    
    m_mutexCommandLineOptionListToProcess.lock();
    if (!m_commandLineOptionListToProcess.isEmpty())
    {
        optionValue = m_commandLineOptionListToProcess.takeFirst();

        optionValueTaken = true;
    }
    m_mutexCommandLineOptionListToProcess.unlock();

    return optionValueTaken;
}

void StarviewerApplicationCommandLine::AddOptionToCommandLineOptionListToProcess(QPair<StarviewerApplicationCommandLine::StarviewerCommandLineOption,QString> optionValue)
{
    m_mutexCommandLineOptionListToProcess.lock();
    m_commandLineOptionListToProcess.append(optionValue);
    m_mutexCommandLineOptionListToProcess.unlock();
}
}
