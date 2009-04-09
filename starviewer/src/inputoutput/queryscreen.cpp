/***************************************************************************
 *   Copyright (C) 2005 by Grup de Gràfics de Girona                       *
 *   http://iiia.udg.es/GGG/index.html?langu=uk                            *
 *                                                                         *
 *   Universitat de Girona                                                 *
 ***************************************************************************/
#include "queryscreen.h"

#include <QMessageBox>
#include <QCloseEvent>
#include <QFileDialog>
#include <QMovie>
#include <QContextMenuEvent>
#include <QShortcut>
#include <QTime>

#include "processimagesingleton.h"
#include "pacsconnection.h"
#include "qstudytreewidget.h"
#include "dicomseries.h"
#include "querypacs.h"
#include "pacsserver.h"
#include "qserieslistwidget.h"
#include "qpacslist.h"
#include "starviewersettings.h"
#include "operation.h"
#include "pacslistdb.h"
#include "logging.h"
#include "status.h"
#include "dicomdirimporter.h"
#include "qcreatedicomdir.h"
#include "dicommask.h"
#include "qoperationstatescreen.h"
#include "localdatabasemanager.h"
#include "patient.h"
#include "starviewerapplication.h"
#include "parsexmlrispierrequest.h"
#include "utils.h"

#ifdef _WIN32
#include <windows.h>
#include <winbase.h>
#endif

namespace udg {

QueryScreen::QueryScreen( QWidget *parent )
 : QDialog(parent )
{
    setupUi( this );
    setWindowFlags( Qt::Widget );
    initialize();//inicialitzem les variables necessàries
    //connectem signals i slots
    createConnections();
    //Fa les comprovacions necessaries per poder executar la QueryScreen de forma correcte   
    checkRequeriments();
    //esborrem els estudis vells de la cache
    deleteOldStudies();
    readSettings();
    //fem que per defecte mostri els estudis de la cache
    queryStudy("Cache");

    // Configuració per Starviewer Lite
#ifdef STARVIEWER_LITE
    m_showPACSNodesToolButton->hide();
    m_operationListToolButton->hide();
    m_createDICOMDIRToolButton->hide();
    m_advancedSearchToolButton->hide();
    m_tab->removeTab(1); // tab de "PACS" fora
#else
    /*L'engeguem després d'haver fet els connects, no es pot fer abans, perquè per exemple en el cas que tinguem un error
     *perquè el port ja està en us, si l'engeguem abans es faria signal indicant error de port en ús i no hi hauria hagut 
     *temps d'haver fet el connect del signal d'error, per tant el signal s'hauria perdut sense poder avisar de l'error
     */
    if (StarviewerSettings().getListenRisRequests()) m_listenRISRequestThread->listen(); 
#endif
}

QueryScreen::~QueryScreen()
{
    /*sinó fem un this.close i tenim la finestra queryscreen oberta al tancar l'starviewer, l'starviewer no finalitza
     *desapareixen les finestres, però el procés continua viu
     */
    this->close();

    delete m_listenRISRequestThread;
}

void QueryScreen::initialize()
{
    StarviewerSettings settings;

    //indiquem que la llista de Pacs no es mostra
    m_showPACSNodes = false;
    m_PACSNodes->setVisible(false);

    /* Posem com a pare el pare de la queryscreen, d'aquesta manera quan es tanqui el pare de la queryscreen
     * el QOperationStateScreen també es tancarà
     */
    m_operationStateScreen = new udg::QOperationStateScreen( this );

    m_qcreateDicomdir = new udg::QCreateDicomdir( this );
    m_processImageSingleton = ProcessImageSingleton::getProcessImageSingleton();

    QMovie *operationAnimation = new QMovie(this);
    operationAnimation->setFileName(":/images/loader.gif");
    m_operationAnimation->setMovie(operationAnimation);
    operationAnimation->start();

    m_qadvancedSearchWidget->hide();
    m_operationAnimation->hide();
    m_labelOperation->hide();
    refreshTab( LocalDataBaseTab );

    CreateContextMenuQStudyTreeWidgetCache();
    CreateContextMenuQStudyTreeWidgetPacs();
    CreateContextMenuQStudyTreeWidgetDicomdir();

    setQStudyTreeWidgetColumnsWidth();

    #ifndef STARVIEWER_LITE
    m_listenRISRequestThread = new ListenRISRequestThread(this);
    if (settings.getListenRisRequests()) m_qpopUpRisRequestsScreen = new QPopUpRisRequestsScreen();
    #endif

}

void QueryScreen::CreateContextMenuQStudyTreeWidgetCache()
{
    QAction *action;

    action = m_contextMenuQStudyTreeWidgetCache.addAction( QIcon(":/images/view.png") , tr( "&View" ) , this , SLOT( view() ) , tr("Ctrl+V") );
    (void) new QShortcut( action->shortcut() , this , SLOT( view() ) );

    action = m_contextMenuQStudyTreeWidgetCache.addAction( QIcon(":/images/databaseRemove.png") , tr( "&Delete" ) , this , SLOT( deleteSelectedStudiesInCache() ) , Qt::Key_Delete );
    (void) new QShortcut( action->shortcut() , this , SLOT( deleteSelectedStudiesInCache() ) );

#ifndef STARVIEWER_LITE
    action = m_contextMenuQStudyTreeWidgetCache.addAction( tr( "Send to DICOMDIR List" ) , this , SLOT( convertToDicomdir() ) , tr( "Ctrl+M" ) );
    (void) new QShortcut( action->shortcut() , this , SLOT( convertToDicomdir() ) );
#endif

    //TODO: No funciona correctament el store al PACS per això ho deixem comentat per a que en les pròximes release no aparegui en el menú aquesta opció
    /*action = m_contextMenuQStudyTreeWidgetCache.addAction( QIcon(":/images/store.png") , tr( "Store to PACS" ) , this , SLOT( storeStudiesToPacs() ) , tr( "Ctrl+S" ) );
    (void) new QShortcut( action->shortcut() , this , SLOT( storeStudiesToPacs() ) );
    */
    m_studyTreeWidgetCache->setContextMenu( & m_contextMenuQStudyTreeWidgetCache );//Especifiquem que és el menú per la cache
}

void QueryScreen::CreateContextMenuQStudyTreeWidgetPacs()
{
    QAction *action;

    action = m_contextMenuQStudyTreeWidgetPacs.addAction( QIcon(":/images/view.png") , tr( "&View" ) , this , SLOT( view() ) , tr("Ctrl+V") );
    (void) new QShortcut( action->shortcut() , this , SLOT( view() ) );

    action = m_contextMenuQStudyTreeWidgetPacs.addAction( QIcon(":/images/retrieve.png") , tr("&Retrieve") , this , SLOT( retrieve() ) , tr("Ctrl+R") );
    (void) new QShortcut( action->shortcut() , this , SLOT( retrieve() ) );

    m_studyTreeWidgetPacs->setContextMenu( & m_contextMenuQStudyTreeWidgetPacs ); //Especifiquem que és el menú del PACS
}

void QueryScreen::CreateContextMenuQStudyTreeWidgetDicomdir()
{
    QAction *action;

    action = m_contextMenuQStudyTreeWidgetDicomdir.addAction( QIcon(":/images/view.png") , tr( "&View" ) , this , SLOT( view() ) , tr("Ctrl+V") );
    (void) new QShortcut( action->shortcut() , this , SLOT( view() ) );

    action = m_contextMenuQStudyTreeWidgetDicomdir.addAction( QIcon(":/images/retrieve.png") , tr("&Import") , this , SLOT( importDicomdir() ) , tr("Ctrl+R") );
    (void) new QShortcut( action->shortcut() , this , SLOT( retrieve() ) );

    m_studyTreeWidgetDicomdir->setContextMenu( & m_contextMenuQStudyTreeWidgetDicomdir ); //Especifiquem que es el menu del dicomdir
}

void QueryScreen::setQStudyTreeWidgetColumnsWidth()
{
    StarviewerSettings settings;

    for ( int column = 0; column < m_studyTreeWidgetCache->getNumberOfColumns(); column++)
    {
        m_studyTreeWidgetCache->setColumnWidth( column , settings.getStudyCacheListColumnWidth(column) );
    }

    for ( int column = 0; column < m_studyTreeWidgetPacs->getNumberOfColumns(); column++)
    {
        m_studyTreeWidgetPacs->setColumnWidth( column , settings.getStudyPacsListColumnWidth(column) );
    }

    for ( int column = 0; column < m_studyTreeWidgetDicomdir->getNumberOfColumns(); column++)
    {
        m_studyTreeWidgetDicomdir->setColumnWidth( column , settings.getStudyDicomdirListColumnWidth(column) );
    }
}

void QueryScreen::setSeriesToSeriesListWidgetCache()
{
    QList<Series*> seriesList;
    LocalDatabaseManager localDatabaseManager;
    DicomMask mask;
    QString studyInstanceUID = m_studyTreeWidgetCache->getCurrentStudyUID();

    INFO_LOG("Cerca de sèries a la cache de l'estudi " + studyInstanceUID);

    //preparem la mascara i cerquem les series a la cache
    mask.setStudyUID(studyInstanceUID);

    seriesList = localDatabaseManager.querySeries(mask);
    if (showDatabaseManagerError( localDatabaseManager.getLastError() ))    return;

    m_seriesListWidgetCache->clear();

    foreach(Series* series, seriesList)
    {
        m_seriesListWidgetCache->insertSeries(studyInstanceUID, series);
    }
}

void QueryScreen::deleteOldStudies()
{
    StarviewerSettings settings;

    /*Mirem si està activada la opció de la configuració d'esborrar els estudis vells no visualitzats en un número de dies determinat
      fem la comprovació, per evitar engegar el thread si no s'han d'esborrar els estudis vells*/
    if (settings.getDeleteOldStudiesHasNotViewedInDays())
    {
        m_qdeleteOldStudiesThread.deleteOldStudies();
    }
}

void QueryScreen::checkRequeriments()
{
    //Comprova que la base de dades d'imatges estigui consistent, comprovant que no haguessin quedat estudis a mig descarregar l'última vegada que es va tancar l'starviewer, i si és així esborra les imatges i deixa la base de dades en un estat consistent
    checkDatabaseImageIntegrity();
    //Comprova que el port pel qual es reben les descàrregues d'objectes dicom del PACS no estigui ja en ús
    checkIncomingConnectionsPacsPortNotInUse();
}

void QueryScreen::checkIncomingConnectionsPacsPortNotInUse()
{
    StarviewerSettings settings;

    if (Utils::isPortInUse(settings.getLocalPort().toInt()))
    {
        QString message = tr("Port %1 for incoming connections from PACS is already in use by another application.").arg(settings.getLocalPort());
        message += tr("\n\n%1 couldn't retrieve studies from PACS if the port is in use, please close the application that is using port %2 or change Starviewer port for incoming connections from PACS in the configuration screen.").arg(ApplicationNameString, settings.getLocalPort());
        message += tr("\n\nIf the error has ocurred when openned new %1's windows, close this window. To open new %1 window you have to choose the 'New' option from the File menu.").arg(ApplicationNameString);

        QMessageBox::warning(this, ApplicationNameString, message);
    }
}

void QueryScreen::checkDatabaseImageIntegrity()
{
    LocalDatabaseManager localDatabaseManager;

    localDatabaseManager.checkNoStudiesRetrieving();

    if (localDatabaseManager.getLastError() != LocalDatabaseManager::Ok)
        showDatabaseManagerError(localDatabaseManager.getLastError(), tr("deleting a study not full retrived"));
}

void QueryScreen::updateOperationsInProgressMessage()
{
    if (m_operationStateScreen->getActiveOperationsCount() > 0)
    {
        m_operationAnimation->show();
        m_labelOperation->show();
    }
    else
    {
        m_operationAnimation->hide();
        m_labelOperation->hide();
    }
}

void QueryScreen::createConnections()
{
    //connectem els butons
    connect( m_searchButton, SIGNAL( clicked() ), SLOT( searchStudy() ) );
    connect( m_clearToolButton, SIGNAL( clicked() ), SLOT( clearTexts() ) );
    connect( m_retrieveButtonPACS, SIGNAL( clicked() ), SLOT( retrieve() ) );
    connect( m_retrieveButtonDICOMDIR, SIGNAL( clicked() ), SLOT( importDicomdir() ) );
    connect( m_operationListToolButton, SIGNAL( clicked() ) , SLOT( showOperationStateScreen() ) );
    connect( m_showPACSNodesToolButton, SIGNAL( toggled(bool) ), m_PACSNodes, SLOT( setVisible(bool) ) );

    connect( m_viewButtonLocal, SIGNAL( clicked() ), SLOT( view() ) );
    connect( m_viewButtonPACS, SIGNAL( clicked() ), SLOT( view() ) );
    connect( m_viewButtonDICOMDIR, SIGNAL( clicked() ), SLOT( view() ) );
    connect( m_createDICOMDIRToolButton, SIGNAL( clicked() ), m_qcreateDicomdir, SLOT( show() ) );

    //connectem Slots dels StudyTreeWidget amb la interficie
    connect( m_studyTreeWidgetPacs, SIGNAL( studyExpanded( QString , QString ) ), SLOT( expandStudy( QString , QString ) ) );
    connect( m_studyTreeWidgetPacs, SIGNAL( seriesExpanded( QString , QString , QString ) ), SLOT( expandSeries( QString , QString , QString ) ) );
    connect( m_studyTreeWidgetPacs, SIGNAL( studyDoubleClicked() ), SLOT( retrieve() ) );
    connect( m_studyTreeWidgetPacs, SIGNAL( seriesDoubleClicked() ), SLOT( retrieve() ) );
    connect( m_studyTreeWidgetPacs, SIGNAL( imageDoubleClicked() ), SLOT( retrieve() ) );

    connect( m_studyTreeWidgetCache, SIGNAL( studyExpanded( QString , QString ) ), SLOT( expandStudy( QString , QString ) ) );
    connect( m_studyTreeWidgetCache, SIGNAL( seriesExpanded( QString , QString , QString ) ), SLOT( expandSeries( QString , QString , QString ) ) );
    connect( m_studyTreeWidgetCache, SIGNAL( studyDoubleClicked() ), SLOT( view() ) );
    connect( m_studyTreeWidgetCache, SIGNAL( seriesDoubleClicked() ), SLOT( view() ) );
    connect( m_studyTreeWidgetCache, SIGNAL( imageDoubleClicked() ), SLOT( view() ) );

    connect( m_studyTreeWidgetDicomdir, SIGNAL( studyExpanded( QString , QString ) ), SLOT( expandStudy( QString , QString ) ) );
    connect( m_studyTreeWidgetDicomdir, SIGNAL( seriesExpanded( QString , QString , QString ) ), SLOT( expandSeries( QString , QString , QString ) ) );
    connect( m_studyTreeWidgetDicomdir, SIGNAL( studyDoubleClicked() ), SLOT( view() ) );
    connect( m_studyTreeWidgetDicomdir, SIGNAL( seriesDoubleClicked() ), SLOT( view() ) );
    connect( m_studyTreeWidgetDicomdir, SIGNAL( imageDoubleClicked() ), SLOT( view() ) );

    //es canvia de pestanya del TAB
    connect( m_tab , SIGNAL( currentChanged( int ) ), SLOT( refreshTab( int ) ) );

    //connectem els signes del SeriesIconView StudyListView
    connect( m_seriesListWidgetCache, SIGNAL( selectedSeriesIcon(QString) ), m_studyTreeWidgetCache, SLOT( setCurrentSeries(QString) ) );
    connect( m_seriesListWidgetCache, SIGNAL( viewSeriesIcon() ), SLOT( viewFromQSeriesListWidget() ) );
    connect( m_studyTreeWidgetCache, SIGNAL( currentStudyChanged() ), SLOT( setSeriesToSeriesListWidgetCache() ) );
    connect( m_studyTreeWidgetCache, SIGNAL( currentSeriesChanged(QString) ), m_seriesListWidgetCache, SLOT( setCurrentSeries(QString) ) );
	//Si passem de tenir un element seleccionat a no tenir-ne li diem al seriesListWidget que no mostri cap previsualització
	connect(m_studyTreeWidgetCache, SIGNAL(notCurrentItemSelected()), m_seriesListWidgetCache, SLOT(clear()));

    //connecta el signal que emiteix qexecuteoperationthread, per visualitzar un estudi amb aquesta classe
    connect( &m_qexecuteOperationThread, SIGNAL( viewStudy( QString , QString , QString ) ), SLOT( studyRetrievedView( QString , QString , QString ) ) , Qt::QueuedConnection );

    //connecta els signals el qexecute operation thread amb els de qretrievescreen, per coneixer quant s'ha descarregat una imatge, serie, estudi, si hi ha error, etc..
    connect( &m_qexecuteOperationThread, SIGNAL( setErrorOperation( QString ) ), m_operationStateScreen, SLOT(  setErrorOperation( QString ) ) );
    connect(&m_qexecuteOperationThread, SIGNAL(errorInOperation(QString, QString, QExecuteOperationThread::OperationError)), m_operationStateScreen, SLOT(setErrorOperation(QString)));

    connect( &m_qexecuteOperationThread, SIGNAL( setOperationFinished( QString ) ), m_operationStateScreen, SLOT(  setOperationFinished( QString ) ) );

    connect( &m_qexecuteOperationThread, SIGNAL( setOperating( QString ) ), m_operationStateScreen, SLOT(  setOperating( QString ) ) );
    connect( &m_qexecuteOperationThread, SIGNAL( imageCommit( QString , int) ), m_operationStateScreen, SLOT(  imageCommit( QString , int ) ) );
    connect( &m_qexecuteOperationThread, SIGNAL( currentProcessingStudyImagesRetrievedChanged(int)), m_operationStateScreen, SLOT( setRetrievedImagesToCurrentProcessingStudy(int) ));
    connect( &m_qexecuteOperationThread, SIGNAL( seriesCommit( QString ) ), m_operationStateScreen, SLOT(  seriesCommit( QString ) ) );
    connect( &m_qexecuteOperationThread, SIGNAL( newOperation( Operation * ) ), m_operationStateScreen, SLOT(  insertNewOperation( Operation *) ) );
    connect(&m_qexecuteOperationThread, SIGNAL(studyWillBeDeleted(QString)), this, SLOT(studyWillBeDeletedSlot(QString)));
    connect(&m_qexecuteOperationThread, SIGNAL(setCancelledOperation(QString)), m_operationStateScreen, SLOT(setCancelledOperation(QString)));

    // Label d'informació (cutre-xapussa)
    connect(&m_qexecuteOperationThread, SIGNAL(errorInOperation(QString, QString, QExecuteOperationThread::OperationError)), SLOT( updateOperationsInProgressMessage()));
    connect( &m_qexecuteOperationThread, SIGNAL( setErrorOperation(QString) ), SLOT( updateOperationsInProgressMessage() ));
    connect( &m_qexecuteOperationThread, SIGNAL( setOperationFinished(QString) ), SLOT( updateOperationsInProgressMessage() ));
    connect( &m_qexecuteOperationThread, SIGNAL( newOperation(Operation *) ), SLOT( updateOperationsInProgressMessage() ));
    connect( &m_qexecuteOperationThread, SIGNAL( setCancelledOperation(QString) ), SLOT( updateOperationsInProgressMessage() ));

    //connect tracta els errors de connexió al PACS
    connect ( &m_multipleQueryStudy, SIGNAL( errorConnectingPacs( QString ) ), SLOT( errorConnectingPacs( QString ) ) );
    connect ( &m_multipleQueryStudy, SIGNAL( errorQueringStudiesPacs( QString ) ), SLOT( errorQueringStudiesPacs( QString ) ) );

    //connect tracta els errors de connexió al PACS, al descarregar imatges
    connect (&m_qexecuteOperationThread, SIGNAL(errorInOperation(QString, QString, QExecuteOperationThread::OperationError)), SLOT(showQExecuteOperationThreadError(QString, QString, QExecuteOperationThread::OperationError)));
    connect( &m_qexecuteOperationThread, SIGNAL( retrieveFinished( QString ) ), SLOT( studyRetrieveFinished ( QString ) ) );

    //Amaga o ensenya la cerca avançada
    connect( m_advancedSearchToolButton, SIGNAL( toggled( bool ) ), SLOT( setAdvancedSearchVisible( bool ) ) );

    //Connecta amb el signal que indica que ha finalitza el thread d'esborrar els estudis vells
    connect(&m_qdeleteOldStudiesThread, SIGNAL(finished()), SLOT(deleteOldStudiesThreadFinished()));

    // connectem el botó per obrir DICOMDIR
    connect( m_openDICOMDIRToolButton, SIGNAL( clicked() ), SLOT( openDicomdir() ) );

    #ifndef STARVIEWER_LITE
    connect(m_listenRISRequestThread, SIGNAL(requestRetrieveStudy(DicomMask)), SLOT(retrieveStudyFromRISRequest(DicomMask)));
    connect(m_listenRISRequestThread, SIGNAL(errorListening(ListenRISRequestThread::ListenRISRequestThreadError)), SLOT(showListenRISRequestThreadError(ListenRISRequestThread::ListenRISRequestThreadError)));
    #endif
}

void QueryScreen::setAdvancedSearchVisible(bool visible)
{
    m_qadvancedSearchWidget->setVisible(visible);

    if (visible)
    {
        m_advancedSearchToolButton->setText( m_advancedSearchToolButton->text().replace(">>","<<") );
    }
    else
    {
        m_qadvancedSearchWidget->clear();
        m_advancedSearchToolButton->setText( m_advancedSearchToolButton->text().replace("<<",">>") );
    }
}

void QueryScreen::readSettings()
{
    StarviewerSettings settings;
    this->restoreGeometry( settings.getQueryScreenGeometry() );
    if ( !settings.getQueryScreenStudyTreeSeriesListQSplitterState().isEmpty() )
    {
        m_StudyTreeSeriesListQSplitter->restoreState( settings.getQueryScreenStudyTreeSeriesListQSplitterState() );
    }
    //carreguem el processImageSingleton
    m_processImageSingleton->setPath( settings.getCacheImagePath() );
}

void QueryScreen::clearTexts()
{
    m_qbasicSearchWidget->clear();
    m_qadvancedSearchWidget->clear();
}

void QueryScreen::updateConfiguration(const QString &configuration)
{
    if (configuration == "Pacs/ListChanged")
    {
        m_PACSNodes->refresh();
    }
    else if (configuration == "Pacs/CacheCleared")
    {
        m_studyTreeWidgetCache->clear();
        m_seriesListWidgetCache->clear();
    }
}

void QueryScreen::bringToFront()
{
    this->show();
    this->raise();
    this->activateWindow();
}

void QueryScreen::searchStudy()
{
    switch ( m_tab->currentIndex() )
    {
        case LocalDataBaseTab:
            queryStudy("Cache");
            break;

        case PACSQueryTab:
            queryStudyPacs();
        break;

        case DICOMDIRTab:
            queryStudy("DICOMDIR");
            break;
    }
}

PacsServer QueryScreen::getPacsServerByPacsID(QString pacsID)
{
    PacsParameters pacsParameters;
    PacsListDB pacsListDB;
    pacsParameters = pacsListDB.queryPacs(pacsID);//cerquem els paràmetres del Pacs 

    StarviewerSettings settings;
    pacsParameters.setAELocal( settings.getAETitleMachine() ); //especifiquem el nostres AE
    pacsParameters.setTimeOut( settings.getTimeout().toInt( NULL , 10 ) ); //li especifiquem el TimeOut

    PacsServer pacsServer;
    pacsServer.setPacs( pacsParameters );

    return pacsServer;
}

void QueryScreen::queryStudyPacs()
{
    QList<PacsParameters> selectedPacsList;

    INFO_LOG( "Cerca d'estudis als PACS amb paràmetres " + buildQueryParametersString(buildDicomMask()) );

    selectedPacsList = m_PACSNodes->getSelectedPacs(); //Emplemen el pacsList amb les pacs seleccionats al QPacsList

    if (selectedPacsList.isEmpty()) //es comprova que hi hagi pacs seleccionats
    {
        QMessageBox::warning( this , ApplicationNameString , tr( "Please select a PACS to query" ) );
        return;
    }

    DicomMask searchMask = buildDicomMask();
    bool stopQuery = false;

    if ( searchMask.isAHeavyQuery() )
    {
        QMessageBox::StandardButton response = QMessageBox::question(this, ApplicationNameString,
                                                                     tr("This query can take a long time.\nDo you want continue?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        stopQuery = (response == QMessageBox::No);
    }

    if( !stopQuery )
    {
        QApplication::setOverrideCursor( QCursor( Qt::WaitCursor ) );

        Status queryStatus = queryMultiplePacs(searchMask, selectedPacsList, &m_multipleQueryStudy);

        if( !queryStatus.good() )  //no fem la query
        {
            m_studyTreeWidgetPacs->clear();
            QApplication::restoreOverrideCursor();
            QMessageBox::information( this , ApplicationNameString , tr( "An error has produced while querying.\n\nRepeat it, if the problem persist contact with an administrator." ) );
            return;
        }

        if (m_multipleQueryStudy.getStudyList().isEmpty() )
        {
            m_studyTreeWidgetPacs->clear();
            QApplication::restoreOverrideCursor();
            QMessageBox::information( this , ApplicationNameString , tr( "No study match found." ) );
            return;
        }

        m_studyListQueriedPacs = m_multipleQueryStudy.getStudyList(); //Guardem una còpia en local de la llista d'estudis trobats al PACS

        m_studyTreeWidgetPacs->insertStudyList( m_multipleQueryStudy.getStudyList() ); //fem que es visualitzi l'studyView seleccionat
        m_studyTreeWidgetPacs->insertSeriesList( m_multipleQueryStudy.getSeriesList() );
        m_studyTreeWidgetPacs->insertImageList( m_multipleQueryStudy.getImageList() );
        m_studyTreeWidgetPacs->setSortColumn( QStudyTreeWidget::ObjectName );

        QApplication::restoreOverrideCursor();
    }
}

Status QueryScreen::queryMultiplePacs(DicomMask searchMask, QList<PacsParameters> listPacsToQuery, MultipleQueryStudy *multipleQueryStudy)
{
    QList<PacsParameters> filledPacsParameters;
    StarviewerSettings settings;

    //TODO PacsParameters no hauria de contenir el AETitle i el timeout
    //Hem d'afegir a les dades de pacs parameters el nostre aetitle i timeout
    foreach(PacsParameters pacs, listPacsToQuery)
    {
        pacs.setAELocal(settings.getAETitleMachine());
        pacs.setTimeOut(settings.getTimeout().toInt());
        filledPacsParameters.append(pacs);
    }

    multipleQueryStudy->setMask( searchMask ); //assignem la mascara
    multipleQueryStudy->setPacsList(filledPacsParameters);
    return multipleQueryStudy->StartQueries();
}

void QueryScreen::queryStudy( QString source )
{
    LocalDatabaseManager localDatabaseManager;
    QList<DICOMStudy> studyListResultQuery;
    QList<Patient*> patientList;
    Status state;

    INFO_LOG( "Cerca d'estudis a la font" + source + " amb paràmetres " + buildQueryParametersString(buildDicomMask()) );
    QApplication::setOverrideCursor( QCursor( Qt::WaitCursor ) );

    if( source == "Cache" )
    {
        m_seriesListWidgetCache->clear();

        patientList = localDatabaseManager.queryPatientStudy(buildDicomMask());

        if (showDatabaseManagerError( localDatabaseManager.getLastError() ))    return;
    }
    else if( source == "DICOMDIR" )
    {
        state = m_readDicomdir.readStudies( studyListResultQuery , buildDicomMask() );
        if ( !state.good() )
        {
            QApplication::restoreOverrideCursor();
            if ( state.code() == 1302 ) //Aquest és l'error quan no tenim un dicomdir obert l'ig
            {
                QMessageBox::warning( this , ApplicationNameString , tr( "Before search you have to open a dicomdir." ) );
                ERROR_LOG( "No s'ha obert cap directori dicomdir " + state.text() );
            }
            else
            {
                QMessageBox::warning( this , ApplicationNameString , tr( "Error quering in dicomdir" ) );
                ERROR_LOG( "Error cercant estudis al dicomdir " + state.text() );
            }
            return;
        }
    }
    else
    {
        QApplication::restoreOverrideCursor();
        DEBUG_LOG( "Unrecognised source: " + source );
        return;
    }

    /* Aquest mètode a part de ser cridada quan l'usuari fa click al botó search, també es cridada al
     * constructor d'aquesta classe, per a que al engegar l'aplicació ja es mostri la llista d'estudis
     * que hi ha a la base de dades local. Si el mètode no troba cap estudi a la base de dades local
     * es llença el missatge que no s'han trobat estudis, però com que no és idonii, en el cas aquest que es
     * crida des del constructor que es mostri el missatge de que no s'han trobat estudis al engegar l'aplicació, el que
     * es fa és que per llançar el missatge es comprovi que la finestra estigui activa. Si la finestra no està activa
     * vol dir que el mètode ha estat invocat des del constructor
     */
    if (patientList.isEmpty() && studyListResultQuery.isEmpty() && isActiveWindow() )
    {
        //no hi ha estudis
        if( source == "Cache" )
            m_studyTreeWidgetCache->clear();
        else if( source == "DICOMDIR" )
            m_studyTreeWidgetDicomdir->clear();

        QApplication::restoreOverrideCursor();
        QMessageBox::information( this , ApplicationNameString , tr( "No study match found." ) );
    }
    else
    {
        if( source == "Cache" )
        {
            m_studyTreeWidgetCache->insertPatientList(patientList);//es mostra la llista d'estudis
            m_studyTreeWidgetCache->setSortColumn( QStudyTreeWidget::ObjectName ); //ordenem pel nom
        }
        else if( source == "DICOMDIR" )
        {
            m_studyTreeWidgetDicomdir->insertStudyList( studyListResultQuery );
            m_studyTreeWidgetDicomdir->setSortColumn( QStudyTreeWidget::ObjectName );//ordenem pel nom
        }
        QApplication::restoreOverrideCursor();
    }
}

void QueryScreen::expandStudy( const QString &studyUID, const QString &pacsId )
{
    QApplication::setOverrideCursor( QCursor( Qt::WaitCursor ) );

    switch ( m_tab->currentIndex() )
    {
        case LocalDataBaseTab : // si estem a la pestanya de la cache
            querySeries( studyUID, "Cache" );
            break;
        case PACSQueryTab :  //si estem la pestanya del PACS fem query al Pacs
            querySeriesPacs(studyUID, pacsId);
            break;
        case DICOMDIRTab : //si estem a la pestanya del dicomdir, fem query al dicomdir
            querySeries( studyUID, "DICOMDIR" );
            break;
    }

    QApplication::restoreOverrideCursor();
}

/* AQUESTA ACCIO ES CRIDADA DES DEL STUDYLISTVIEW*/
void QueryScreen::expandSeries( const QString &studyUID, const QString &seriesUID, const QString &pacsId )
{
    QApplication::setOverrideCursor( QCursor( Qt::WaitCursor ) );

    switch ( m_tab->currentIndex() )
    {
        case LocalDataBaseTab : // si estem a la pestanya de la cache
            queryImage( studyUID , seriesUID, "Cache" );
            break;

        case PACSQueryTab :  //si estem la pestanya del PACS fem query al Pacs
            queryImagePacs( studyUID , seriesUID , pacsId);
            break;

        case DICOMDIRTab : //si estem a la pestanya del dicomdir, fem query al dicomdir
            queryImage( studyUID , seriesUID, "DICOMDIR" );
            break;
    }

    QApplication::restoreOverrideCursor();
}

void QueryScreen::querySeriesPacs(QString studyUID, QString pacsID)
{
    DICOMSeries serie;
    Status state;
    QString text, pacsDescription;
    QueryPacs querySeriesPacs;

    PacsServer pacsServer = getPacsServerByPacsID(pacsID);
    pacsDescription = pacsServer.getPacs().getAEPacs() + " Institució" + pacsServer.getPacs().getInstitution()  + " IP:" + pacsServer.getPacs().getPacsAddress(); 

    INFO_LOG("Cercant informacio de les sèries de l'estudi" + studyUID + " del PACS " + pacsDescription);


    state = pacsServer.connect(PacsServer::query, PacsServer::seriesLevel);
    if ( !state.good() )
    {
        //Error al connectar
        ERROR_LOG( "Error al connectar al PACS " + pacsDescription + ". PACS ERROR : " + state.text() );
        errorConnectingPacs (pacsID);
        return;
    }

    querySeriesPacs.setConnection(pacsID, pacsServer.getConnection());
    state = querySeriesPacs.query( buildSeriesDicomMask( studyUID ) );
    pacsServer.disconnect();

    if ( !state.good() )
    {
        //Error a la query
        ERROR_LOG( "QueryScreen::QueryPacs : Error cercant les sèries al PACS " + pacsDescription + ". PACS ERROR : " + state.text() );

        text = tr( "Error! Can't query series to PACS named %1" ).arg( pacsDescription );
        QMessageBox::warning( this , ApplicationNameString , text );
        return;
    }

    if ( querySeriesPacs.getQueryResultsAsSeriesList().isEmpty() )
    {
        QMessageBox::information( this , ApplicationNameString , tr( "No series match for this study.\n" ) );
        return;
    }

    m_studyTreeWidgetPacs->insertSeriesList( querySeriesPacs.getQueryResultsAsSeriesList() );
}

void QueryScreen::querySeries( QString studyUID, QString source )
{
    QList<DICOMSeries> seriesListQueryResults;
    QList<Series*> seriesList;
    LocalDatabaseManager localDatabaseManager;
    DicomMask mask;

    INFO_LOG( "Cerca de sèries a la font " + source +" de l'estudi " + studyUID );
    if( source == "Cache" )
    {
        //preparem la mascara i cerquem les series a la cache
        mask.setStudyUID(studyUID);
        seriesList = localDatabaseManager.querySeries(mask);
        if (showDatabaseManagerError( localDatabaseManager.getLastError() ))    return;
    }
    else if( source == "DICOMDIR" )
    {
        m_readDicomdir.readSeries( studyUID , "" , seriesListQueryResults ); //"" pq no busquem cap serie en concret
    }
    else
    {
        DEBUG_LOG( "Unrecognised source: " + source );
        return;
    }

    if ( seriesListQueryResults.isEmpty() && seriesList.isEmpty() )
    {
        QMessageBox::information( this , ApplicationNameString , tr( "No series match for this study.\n" ) );
        return;
    }

    if( source == "Cache" )
    {
        m_studyTreeWidgetCache->insertSeriesList(studyUID, seriesList); //inserim la informació de les sèries al estudi
    }
    else if( source == "DICOMDIR" )
        m_studyTreeWidgetDicomdir->insertSeriesList( seriesListQueryResults );//inserim la informació de la sèrie al llistat
}

void QueryScreen::queryImagePacs(QString studyUID, QString seriesUID, QString pacsID)
{
    DICOMSeries serie;
    Status state;
    QString text, pacsDescription;
    QueryPacs queryImages;
    DicomMask dicomMask;

    QApplication::setOverrideCursor( QCursor( Qt::WaitCursor ) );

    PacsServer pacsServer = getPacsServerByPacsID(pacsID);
    pacsDescription = pacsServer.getPacs().getAEPacs() + " Institució" + pacsServer.getPacs().getInstitution()  + " IP:" + pacsServer.getPacs().getPacsAddress(); 

    INFO_LOG("Cercant informacio de les imatges de l'estudi" + studyUID + " serie " + seriesUID + " del PACS " + pacsDescription);

    dicomMask.setStudyUID( studyUID );
    dicomMask.setSeriesUID( seriesUID );
    dicomMask.setImageNumber( "" );
    dicomMask.setSOPInstanceUID( "" );

    state = pacsServer.connect(PacsServer::query,PacsServer::imageLevel);
    if ( !state.good() )
    {   //Error al connectar
        QApplication::restoreOverrideCursor();
        ERROR_LOG( "Error al connectar al PACS " + pacsDescription + ". PACS ERROR : " + state.text() );
        errorConnectingPacs (pacsID);
        return;
    }

    queryImages.setConnection(pacsID, pacsServer.getConnection());

    state = queryImages.query( dicomMask );
    if ( !state.good() )
    {
        //Error a la query
        QApplication::restoreOverrideCursor();
        ERROR_LOG( "QueryScreen::QueryPacs : Error cercant les images al PACS " + pacsDescription + ". PACS ERROR : " + state.text() );

        text = tr( "Error! Can't query images to PACS named %1 " ).arg(pacsDescription);
        QMessageBox::warning( this , ApplicationNameString , text );
        return;
    }

    pacsServer.disconnect();

    if ( queryImages.getQueryResultsAsImageList().isEmpty() )
    {
        QApplication::restoreOverrideCursor();
        QMessageBox::information( this , ApplicationNameString , tr( "No images match for this series.\n" ) );
        return;
    }

    m_studyTreeWidgetPacs->insertImageList( queryImages.getQueryResultsAsImageList() );

    QApplication::restoreOverrideCursor();
}

void QueryScreen::retrieve(bool view)
{
    QStringList selectedStudiesUIDList = m_studyTreeWidgetPacs->getSelectedStudiesUID();

    if( selectedStudiesUIDList.isEmpty() )
    {
        QApplication::restoreOverrideCursor();
        if( view )
            QMessageBox::warning( this , ApplicationNameString , tr( "Select a study to view " ) );
        else
            QMessageBox::warning( this , ApplicationNameString , tr( "Select a study to download " ) );

        return;
    }

    foreach( QString currentStudyUID, selectedStudiesUIDList)
    {
        DicomMask maskStudyToRetrieve;

        //Tenim l'informació de l'estudi a descarregar a la llista d'estudis cercats del pacs, el busquem a la llista a través d'aquest mètode
        //TODO no hauria de tenir la responsabilitat de retornar l'estudi al QStudyTreeView no la pròpia QueryScreen
        int indexStudyInList = getStudyPositionInStudyListQueriedPacs(currentStudyUID, 
        m_studyTreeWidgetPacs->getStudyPACSIdFromSelectedItems(currentStudyUID)); 

        //Es comprova que existeixi l'estudi a la llista d'estudis de la última query que s'ha fet al PACS
        if ( indexStudyInList == -1 ) 
        {
            QApplication::restoreOverrideCursor();
            QMessageBox::warning(this , ApplicationNameString, tr("Internal Error: %2 can't retrieve study with UID %1, because can't find study information.").arg(currentStudyUID, ApplicationNameString));
            ERROR_LOG("No s'ha trobat la informació de l'estudi a la llista m_studyTreeWidgetPacs per poder descarregar l'estudi");
        }
        else
        {
            maskStudyToRetrieve.setStudyUID(currentStudyUID);

            // TODO aquí només tenim en compte l'última sèrie o imatge seleccionada
            // per tant si seleccionem més d'una sèrie/imatge només s'en baixarà una
            // Caldria fer possible que es baixi tants com en seleccionem
            if ( !m_studyTreeWidgetPacs->getCurrentSeriesUID().isEmpty() )
                maskStudyToRetrieve.setSeriesUID( m_studyTreeWidgetPacs->getCurrentSeriesUID() );

            if ( !m_studyTreeWidgetPacs->getCurrentImageUID().isEmpty() )
                maskStudyToRetrieve.setSOPInstanceUID( m_studyTreeWidgetPacs->getCurrentImageUID() );

            retrieveFromPacs(view, m_studyTreeWidgetPacs->getStudyPACSIdFromSelectedItems(currentStudyUID), maskStudyToRetrieve, m_studyListQueriedPacs.value(indexStudyInList));
        }
    }
}

void QueryScreen::queryImage(QString studyInstanceUID, QString seriesInstanceUID, QString source)
{
    LocalDatabaseManager localDatabaseManager;
    DicomMask mask;
    QList<Image*> imageList;
    QList<DICOMImage> imageListQueryResults;

    INFO_LOG("Cerca d'imatges a la font " + source + " de l'estudi " + studyInstanceUID + " i serie " + seriesInstanceUID);

    if(source == "Cache")
    {
        mask.setStudyUID(studyInstanceUID);
        mask.setSeriesUID(seriesInstanceUID);
        imageList = localDatabaseManager.queryImage(mask);
        if(showDatabaseManagerError(localDatabaseManager.getLastError()))   return;
    }
    else if (source == "DICOMDIR")
    {
        m_readDicomdir.readImages(seriesInstanceUID, "", imageListQueryResults);
    }
    else
    {
        DEBUG_LOG("Unrecognised source: " + source);
        return;
    }

    if (imageListQueryResults.isEmpty() && imageList.isEmpty())
    {
        QMessageBox::information( this , ApplicationNameString , tr( "No images match for this study.\n" ) );
        return;
    }

    if( source == "Cache" )
        m_studyTreeWidgetCache->insertImageList(studyInstanceUID, seriesInstanceUID, imageList);
    else if( source == "DICOMDIR" )
        m_studyTreeWidgetDicomdir->insertImageList( imageListQueryResults );//inserim la informació de la sèrie al llistat
}

void QueryScreen::retrieveFromPacs(bool view, QString pacsIdToRetrieve, DicomMask maskStudyToRetrieve, DICOMStudy studyToRetrieve)
{
    StarviewerSettings settings;
    QString defaultSeriesUID;
    Operation operation;
    PacsParameters pacs;

    QApplication::setOverrideCursor( QCursor ( Qt::WaitCursor ) );

    //busquem els paràmetres del pacs del qual volem descarregar l'estudi
    PacsListDB pacsListDB;
    pacs = pacsListDB.queryPacs(pacsIdToRetrieve);

    //emplanem els parametres locals per conenctar amb el pacs amb dades del starviewersettings
    pacs.setAELocal( settings.getAETitleMachine() );
    pacs.setTimeOut( settings.getTimeout().toInt( NULL, 10 ) );
    pacs.setLocalPort( settings.getLocalPort() );

    //definim l'operació
    operation.setPacsParameters( pacs );
    operation.setDicomMask(maskStudyToRetrieve);
    if ( view )
    {
        operation.setOperation( Operation::View );
        operation.setPriority( Operation::Medium );//Té priorita mitjà per passar al davant de les operacions de Retrieve
    }
    else
    {
        operation.setOperation( Operation::Retrieve );
        operation.setPriority( Operation::Low );
    }
    //emplenem les dades de l'operació
    operation.setPatientName( studyToRetrieve.getPatientName() );
    operation.setPatientID( studyToRetrieve.getPatientId() );
    operation.setStudyID( studyToRetrieve.getStudyId() );
    operation.setStudyUID( studyToRetrieve.getStudyUID() );

    m_qexecuteOperationThread.queueOperation( operation );

    QApplication::restoreOverrideCursor();
}

void QueryScreen::studyRetrievedView( QString studyUID , QString seriesUID , QString sopInstanceUID )
{
    QStringList studyUIDList;
    studyUIDList << studyUID;

    loadStudies(studyUIDList, seriesUID, sopInstanceUID, "Cache");
}

void QueryScreen::refreshTab( int index )
{
    switch ( index )
    {
        case LocalDataBaseTab:
                m_qbasicSearchWidget->setEnabledSeriesModality(false);
                m_qadvancedSearchWidget->setEnabled(false);
                break;

        case PACSQueryTab:
                m_qbasicSearchWidget->setEnabledSeriesModality(true);
                m_qadvancedSearchWidget->setEnabled( true );
                break;

        case DICOMDIRTab:
                m_qbasicSearchWidget->setEnabledSeriesModality(false);
                m_qadvancedSearchWidget->setEnabled(false);
                break;
    }
}

void QueryScreen::view()
{
    switch ( m_tab->currentIndex() )
    {
        case LocalDataBaseTab :
            loadStudies( m_studyTreeWidgetCache->getSelectedStudiesUID(), m_studyTreeWidgetCache->getCurrentSeriesUID(), m_studyTreeWidgetCache->getCurrentImageUID(), "Cache" );
            break;

        case PACSQueryTab :
            retrieve(true);
           break;

        case DICOMDIRTab :
            loadStudies( m_studyTreeWidgetDicomdir->getSelectedStudiesUID(), m_studyTreeWidgetDicomdir->getCurrentSeriesUID(), m_studyTreeWidgetDicomdir->getCurrentImageUID(), "DICOMDIR" );
            break;

        default :
            break;
    }
}

void QueryScreen::viewFromQSeriesListWidget()
{
    QStringList studyUIDList;

    studyUIDList << m_seriesListWidgetCache->getCurrentStudyUID();//Agafem l'estudi uid de la sèrie seleccionada
    loadStudies( studyUIDList, m_seriesListWidgetCache->getCurrentSeriesUID(), "", "Cache" );
}

void QueryScreen::deleteOldStudiesThreadFinished()
{
    showDatabaseManagerError(m_qdeleteOldStudiesThread.getLastError(), tr("deleting old studies"));
}

void QueryScreen::loadStudies(QStringList studiesUIDList, QString defaultSeriesUID, QString defaultSOPInstanceUID, QString source)
{
    DicomMask patientToProcessMask;
    Patient *patient;
    QTime time;

    if(studiesUIDList.isEmpty())
    {
        QMessageBox::warning(this, ApplicationNameString, tr("Select at least one study to view"));
        return;
    }

    this->close();//s'amaga per poder visualitzar la serie
    if ( m_operationStateScreen->isVisible() )
    {
        m_operationStateScreen->close();//s'amaga per poder visualitzar la serie
    }

    QApplication::setOverrideCursor( Qt::WaitCursor );
    // Llista de pacients seleccionats
    QList<Patient *> selectedPatientsList;
    //TODO: S'hauria de millorar el mètode ja que per la seva estructura lo d'obrir l'estudi per la sèrie que ens tinguin seleccionada només ho farà per un estudi ja que aquest mètode només se li passa per paràmetre una sèrie per defecte
    foreach(QString studyInstanceUIDSelected, studiesUIDList)
    {
        patientToProcessMask.setStudyUID(studyInstanceUIDSelected);
        if( source == "Cache" )
        {
            LocalDatabaseManager localDatabaseManager;

            time.start();
            patient = localDatabaseManager.retrieve(patientToProcessMask);
            DEBUG_LOG( QString("Rehidratar de la bd ha trigat: %1 ").arg( time.elapsed() ));
            if(showDatabaseManagerError(localDatabaseManager.getLastError()))
            {
                QApplication::restoreOverrideCursor();
                return;
            }
        }
        else if(source == "DICOMDIR")
        {
            time.start();
            patient = m_readDicomdir.retrieve(patientToProcessMask);
            DEBUG_LOG( QString("Llegir del DICOMDIR directament (sense importar) ha trigat: %1 ").arg( time.elapsed() ));
        }
        if( patient )
        {
            // Marquem la sèrie per defecte
            // TODO ara sempre posem el mateix UID, per tant de moment només 
            /// funciona bé del tot quan seleccionem un únic estudi
            patient->setSelectedSeries(defaultSeriesUID);
            // l'afegim a la llista de pacients
            selectedPatientsList << patient;
        }
        else
            DEBUG_LOG("No s'ha pogut obtenir l'estudi amb UID " + studyInstanceUIDSelected );
    }

    INFO_LOG("Llançat signal per visualitzar estudi del pacient " + patient->getFullName());
    QApplication::restoreOverrideCursor();
    emit selectedPatients( Patient::mergePatients( selectedPatientsList ) );
}

void QueryScreen::importDicomdir()
{
    DICOMDIRImporter importDicom;

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

//     importDicom.import( m_readDicomdir.getDicomdirPath() , m_studyTreeWidgetDicomdir->getSelectedStudyUID() , m_studyTreeWidgetDicomdir->getSelectedSeriesUID() ,  m_studyTreeWidgetDicomdir->getSelectedImageUID() );
    // TODO ara només permetrem importar estudis sencers
    foreach(QString studyUID, m_studyTreeWidgetDicomdir->getSelectedStudiesUID())
    {
        importDicom.import(m_readDicomdir.getDicomdirFilePath(), studyUID, QString(), QString());
        if (importDicom.getLastError() != DICOMDIRImporter::Ok)
        {
            //S'ha produït un error
            QApplication::restoreOverrideCursor();
            showDICOMDIRImporterError(studyUID, importDicom.getLastError());
            QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

            if (importDicom.getLastError() != DICOMDIRImporter::PatientInconsistent &&
                importDicom.getLastError() != DICOMDIRImporter::DicomdirInconsistent)
            {
                //Si es produeix qualsevol dels altres errors parem d'importar estudis, perquè segurament les següents importacions també fallaran
                break;
            }
        }
    }

	QApplication::restoreOverrideCursor();

    queryStudy("Cache"); //Actualitzem la llista tenint en compte el criteri de cerca
}

void QueryScreen::deleteSelectedStudiesInCache()
{
    QStringList studiesList = m_studyTreeWidgetCache->getSelectedStudiesUID();
    if( studiesList.isEmpty() )
    {
        QMessageBox::information( this , ApplicationNameString , tr( "Please select at least one study to delete" ) );
    }
    else
    {
        QMessageBox::StandardButton response = QMessageBox::question(this, ApplicationNameString,
                                                                           tr( "Are you sure you want to delete the selected Studies?" ),
                                                                           QMessageBox::Yes | QMessageBox::No,
                                                                           QMessageBox::No);
        if (response  == QMessageBox::Yes)
        {
            //Posem el cursor en espera
            QApplication::setOverrideCursor(Qt::WaitCursor);

            LocalDatabaseManager localDatabaseManager;

            foreach(QString studyUID, studiesList)
            {
                if( m_qcreateDicomdir->studyExists( studyUID ) )
                {
                    QMessageBox::warning( this , ApplicationNameString ,
                    tr( "The study with UID: %1 is in use by the DICOMDIR List. If you want to delete this study you should remove it from the DICOMDIR List first." ).arg(studyUID) );
                }
                else
                {
                    localDatabaseManager.del(studyUID);
                    if (showDatabaseManagerError( localDatabaseManager.getLastError() ))    break;

                    m_seriesListWidgetCache->clear();
                    m_studyTreeWidgetCache->removeStudy( studyUID );
                }
            }

            QApplication::restoreOverrideCursor();
        }
    }
}

void QueryScreen::studyRetrieveFinished( QString studyUID )
{
    LocalDatabaseManager localDatabaseManager;
    DicomMask studyMask;
    QList<Patient*> patientList;

    studyMask.setStudyUID(studyUID);
    patientList = localDatabaseManager.queryPatientStudy(studyMask);
    if( showDatabaseManagerError( localDatabaseManager.getLastError() ))    return;

    if (patientList.count() == 1)
    {
        m_studyTreeWidgetCache->insertPatient(patientList.at(0));
        m_studyTreeWidgetCache->sort();
    }
}

void QueryScreen::closeEvent( QCloseEvent* event )
{
    writeSettings(); // guardem els settings

    m_operationStateScreen->close(); //Tanquem la QOperationStateScreen al tancar la QueryScreen

    event->accept();
    m_qcreateDicomdir->clearTemporaryDir();
}

void QueryScreen::writeSettings()
{
    /* Només guardem els settings quan la interfície ha estat visible, ja que hi ha settings com el QSplitter que si la primera vegada
     * que executem l'starviewer no obrim la QueryScreen retorna un valors incorrecte per això, el que fem és comprova que la QueryScreen
     * hagi estat visible per guardar el settings
     */
    if (this->isVisible())
    {
        StarviewerSettings settings;

        saveQStudyTreeWidgetColumnsWidth();

        //guardem l'estat del QSplitter que divideix el StudyTree del QSeries i de la QueryScreen
        settings.setQueryScreenStudyTreeSeriesListQSplitterState( m_StudyTreeSeriesListQSplitter->saveState() );
        settings.saveQueryScreenGeometry( this->saveGeometry() );
    }
}

void QueryScreen::saveQStudyTreeWidgetColumnsWidth()
{
    StarviewerSettings settings;

    for ( int column = 0; column < m_studyTreeWidgetPacs->getNumberOfColumns(); column++ )
    {
        settings.setStudyPacsListColumnWidth( column , m_studyTreeWidgetPacs->getColumnWidth( column ) );
    }

    for ( int column = 0; column < m_studyTreeWidgetCache->getNumberOfColumns(); column++ )
    {
        settings.setStudyCacheListColumnWidth( column , m_studyTreeWidgetCache->getColumnWidth( column ) );
    }

    for ( int column = 0; column < m_studyTreeWidgetDicomdir->getNumberOfColumns(); column++ )
    {
        settings.setStudyDicomdirListColumnWidth( column , m_studyTreeWidgetDicomdir->getColumnWidth( column ) );
    }
}

void QueryScreen::showOperationStateScreen()
{
    if ( !m_operationStateScreen->isVisible() )
    {
        m_operationStateScreen->setVisible( true );
    }
    else
    {
        m_operationStateScreen->raise();
        m_operationStateScreen->activateWindow();
    }
}

void QueryScreen::convertToDicomdir()
{
    QStringList studiesUIDList = m_studyTreeWidgetCache->getSelectedStudiesUID();

    DicomMask studyMask;
    LocalDatabaseManager localDatabaseManager;
    QList<Patient*> patientList;

    foreach(QString studyUID, studiesUIDList )
    {
        studyMask.setStudyUID(studyUID);
        patientList = localDatabaseManager.queryPatientStudy(studyMask);
        if( showDatabaseManagerError( localDatabaseManager.getLastError() ))    return;

        // \TODO Això s'ha de fer perquè queryPatientStudy retorna llista de Patients
        // Nosaltres, en realitat, volem llista d'study amb les dades de Patient omplertes.
        if(patientList.size() != 1 && patientList.first()->getNumberOfStudies() != 1)
        {
            showDatabaseManagerError(LocalDatabaseManager::DatabaseCorrupted);
            return;
        }

        m_qcreateDicomdir->addStudy(patientList.first()->getStudies().first());

        delete patientList.first();
   }
}

void QueryScreen::openDicomdir()
{
    StarviewerSettings settings;
    QString dicomdirPath;
    Status state;
    
    dicomdirPath = QFileDialog::getOpenFileName(0, QFileDialog::tr( "Open" ), settings.getLastOpenedDICOMDIRPath(), "DICOMDIR");

    if (!dicomdirPath.isEmpty())//Si és buit no ens han seleccionat cap fitxer
    {
        QApplication::setOverrideCursor( Qt::WaitCursor );
        state = m_readDicomdir.open (dicomdirPath);//Obrim el dicomdir
        QApplication::restoreOverrideCursor();
        if ( !state.good() )
        {
            QMessageBox::warning( this , ApplicationNameString , tr( "Error openning dicomdir" ) );
            ERROR_LOG( "Error al obrir el dicomdir " + dicomdirPath + state.text() );
        }
        else
        {
            INFO_LOG( "Obert el dicomdir " + dicomdirPath );
            settings.setLastOpenedDICOMDIRPath( QFileInfo(dicomdirPath).dir().path() );
            this->bringToFront();
            m_tab->setCurrentIndex( DICOMDIRTab ); // mostre el tab del dicomdir
        }

        clearTexts();//Netegem el filtre de cerca al obrir el dicomdir
        //cerquem els estudis al dicomdir per a que es mostrin
        queryStudy("DICOMDIR");
    }
}

void QueryScreen::storeStudiesToPacs()
{
    QList<PacsParameters> selectedPacsList;
    QStringList studiesUIDList = m_studyTreeWidgetCache->getSelectedStudiesUID();
    QApplication::setOverrideCursor( QCursor( Qt::WaitCursor ) );
    StarviewerSettings settings;

    selectedPacsList = m_PACSNodes->getSelectedPacs(); //Emplemen el pacsList amb les pacs seleccionats al QPacsList

    if(selectedPacsList.size() == 0)
    {
        QApplication::restoreOverrideCursor();
        QMessageBox::warning(this, ApplicationNameString, tr("You have to select a PACS to store the study in") );
    }
    else if(selectedPacsList.size() == 1)
    {
        StarviewerSettings settings;
        foreach( QString studyUID, studiesUIDList )
        {
            PacsListDB pacsListDB;
            PacsParameters pacs;
            Operation storeStudyOperation;
            Study *study;
            LocalDatabaseManager localDatabaseManager;
            QList<Patient*> patientList;

            DicomMask dicomMask;
            dicomMask.setStudyUID(studyUID);
            patientList = localDatabaseManager.queryPatientStudy(dicomMask);
            if( showDatabaseManagerError( localDatabaseManager.getLastError() ))    return;

            // \TODO Això s'ha de fer perquè queryPatientStudy retorna llista de Patients
            // Nosaltres, en realitat, volem llista d'study amb les dades de Patient omplertes.
            if(patientList.size() != 1 && patientList.first()->getNumberOfStudies() != 1)
            {
                showDatabaseManagerError(LocalDatabaseManager::DatabaseCorrupted);
                return;
            }

            study = patientList.first()->getStudies().first();
            Patient *patient = study->getParentPatient();

            storeStudyOperation.setPatientName( patient->getFullName() );
            storeStudyOperation.setPatientID( patient->getID() );
            storeStudyOperation.setStudyUID( study->getInstanceUID() );
            storeStudyOperation.setStudyID( study->getID() );
            storeStudyOperation.setPriority( Operation::Low );
            storeStudyOperation.setOperation( Operation::Move );
            storeStudyOperation.setDicomMask( dicomMask );

            delete patient;
            //cerquem els paràmetres del Pacs al qual s'han de cercar les dades
            pacs = pacsListDB.queryPacs(selectedPacsList.value(0).getPacsID());
            pacs.setAELocal(settings.getAETitleMachine());
            pacs.setTimeOut(settings.getTimeout().toInt());
            storeStudyOperation.setPacsParameters( pacs );

            m_qexecuteOperationThread.queueOperation( storeStudyOperation );
        }
    }
    else
    {
        QApplication::restoreOverrideCursor();
        QMessageBox::warning(this, ApplicationNameString, tr("The studies can only be stored to one PACS") );
    }

    QApplication::restoreOverrideCursor();
}

void QueryScreen::errorConnectingPacs( QString IDPacs )
{
    PacsListDB pacsListDB;
    PacsParameters errorPacs;
    QString errorMessage;

    errorPacs = pacsListDB.queryPacs(IDPacs);

    errorMessage = tr( "Can't connect to PACS %1 from %2.\nBe sure that the IP and AETitle of the PACS are correct." )
        .arg( errorPacs.getAEPacs() )
        .arg( errorPacs.getInstitution()
    );

    QMessageBox::critical( this , ApplicationNameString , errorMessage );
}

void QueryScreen::errorQueringStudiesPacs(QString PacsID)
{
    PacsListDB pacsListDB;
    PacsParameters errorPacs;
    QString errorMessage;

    errorPacs = pacsListDB.queryPacs(PacsID);
    errorMessage = tr( "Can't query PACS %1 from %2\nBe sure that the IP and AETitle of this PACS are correct" )
        .arg( errorPacs.getAEPacs() )
        .arg( errorPacs.getInstitution()
    );

    QMessageBox::critical( this , ApplicationNameString , errorMessage );
}

DicomMask QueryScreen::buildSeriesDicomMask( QString studyUID )
{
    DicomMask mask;

    mask.setStudyUID( studyUID );
    mask.setSeriesDate( "" );
    mask.setSeriesTime( "" );
    mask.setSeriesModality( "" );
    mask.setSeriesNumber( "" );
    mask.setSeriesUID( "" );
    mask.setPPSStartDate( "" );
    mask.setPPStartTime( "" );
    mask.setRequestAttributeSequence( "" , "" );

    return mask;
}

DicomMask QueryScreen::buildDicomMask()
{
    return m_qbasicSearchWidget->buildDicomMask() + m_qadvancedSearchWidget->buildDicomMask();
}

int QueryScreen::getStudyPositionInStudyListQueriedPacs(QString studyUID, QString pacsId)
{
    int index = 0;
    bool studyUIDisTheSame = false , pacsAETitleIsTheSame = false;

    while ( index < m_studyListQueriedPacs.count() && ( !studyUIDisTheSame || !pacsAETitleIsTheSame ) )
    {
        studyUIDisTheSame = m_studyListQueriedPacs.value( index ).getStudyUID() == studyUID;
        pacsAETitleIsTheSame = m_studyListQueriedPacs.value( index ).getPacsId() == pacsId;

        if (!studyUIDisTheSame || !pacsAETitleIsTheSame ) index++;
    }

    return index < m_studyListQueriedPacs.count() ? index : -1;
}

void QueryScreen::studyWillBeDeletedSlot(QString studyInstanceUID)
{
    m_studyTreeWidgetCache->removeStudy(studyInstanceUID);
}

void QueryScreen::retrieveStudyFromRISRequest(DicomMask maskRisRequest)
{
    MultipleQueryStudy multipleQueryStudy;
    DicomMask maskStudyToRetrieve;
    StarviewerSettings settings;

    m_qpopUpRisRequestsScreen->setAccessionNumber(maskRisRequest.getAccessionNumber()); //Mostrem el popUP amb l'accession number

    m_qpopUpRisRequestsScreen->show();

    Status state = queryMultiplePacs(maskRisRequest, PacsListDB().queryDefaultPacs(), &multipleQueryStudy);

    //Fem els connects per tracta els possibles errors que es poden donar
    connect ( &m_multipleQueryStudy, SIGNAL( errorConnectingPacs( QString ) ), SLOT( errorConnectingPacs( QString ) ) );
    connect ( &m_multipleQueryStudy, SIGNAL( errorQueringStudiesPacs( QString ) ), SLOT( errorQueringStudiesPacs( QString ) ) );

    if (!state.good())
    {
        QMessageBox::critical(this , ApplicationNameString , tr("An error ocurred querying default PACS, can't process the RIS request."));
        return;
    }

    if (multipleQueryStudy.getStudyList().isEmpty())
    {
        QString message = tr("%2 can't execute the RIS request, because hasn't found the Study with accession number %1 in the default PACS.").arg(maskRisRequest.getAccessionNumber(), ApplicationNameString);
        QMessageBox::information(this , ApplicationNameString , message);
        return;
    }

    foreach (DICOMStudy study, multipleQueryStudy.getStudyList())
    {
        maskStudyToRetrieve.setStudyUID(study.getStudyUID());
        retrieveFromPacs(settings.getViewAutomaticallyAStudyRetrievedFromRisRequest(), study.getPacsId(), maskStudyToRetrieve, study);
    }
}

QString QueryScreen::buildQueryParametersString(DicomMask mask)
{
    QString logMessage;

    logMessage = "PATIENT_ID=[" + mask.getPatientId() + "] "
        + "PATIENT_NAME=[" + mask.getPatientName() + "] "
        + "STUDY_ID=[" + mask.getStudyId() + "] "
        + "DATES_MASK=[" + mask.getStudyDate() + "] "
        + "ACCESSION_NUMBER=[" + mask.getAccessionNumber() + "]";

    return logMessage;
}

bool QueryScreen::showDatabaseManagerError(LocalDatabaseManager::LastError error, const QString &doingWhat)
{
    QString message;

    if (!doingWhat.isEmpty())
        message = tr("An error has ocurred while ") + doingWhat + ":\n\n";

    switch(error)
    {
        case LocalDatabaseManager::Ok:
            return false;

        case LocalDatabaseManager::DatabaseLocked:
            message += tr("The database is blocked by another %1 window."
                         "\nClose all the others %1 windows and try again."
                         "\n\nIf you want to open different %1's windows always choose the 'New' option from the File menu.").arg(ApplicationNameString);
            break;
        case LocalDatabaseManager::DatabaseCorrupted:
			message += tr("%1 database is corrupted.").arg(ApplicationNameString);
            message += tr("\nClose all %1 windows and try again."
                         "\n\nIf the problem persist contact with an administrator.").arg(ApplicationNameString);
            break;
        case LocalDatabaseManager::SyntaxErrorSQL:
            message += tr("%1 database syntax error.").arg(ApplicationNameString);
            message += tr("\nClose all %1 windows and try again."
                         "\n\nIf the problem persist contact with an administrator.").arg(ApplicationNameString);
            break;
        case LocalDatabaseManager::DatabaseError:
            message += tr("An internal error occurs with %1 database.").arg(ApplicationNameString);
            message += tr("\nClose all %1 windows and try again."
                         "\n\nIf the problem persist contact with an administrator.").arg(ApplicationNameString);
            break;
        case LocalDatabaseManager::DeletingFilesError:
            message += tr("Some files can not be delete."
                         "\nThese have to be delete manually.");
            break;
        default:
            message = tr("Unknown error.");
            break;
    }

    QApplication::restoreOverrideCursor();

    QMessageBox::critical( this , ApplicationNameString , message );

    return true;
}

void QueryScreen::showQExecuteOperationThreadError(QString studyInstanceUID, QString pacsID, QExecuteOperationThread::OperationError error)
{
    QString message;
    StarviewerSettings settings;
    PacsParameters pacs = PacsListDB().queryPacs(pacsID);

    switch (error)
    {
        case QExecuteOperationThread::ErrorConnectingPacs :
            message = tr("Please review the operation list screen, ");
            message += tr("%1 can't connect to PACS %2 trying to retrieve or store a study.\n").arg(ApplicationNameString, pacs.getAEPacs());
            message += tr("\nBe sure that your computer is connected on network and the Pacs parameters are correct.");
            message += tr("\nIf the problem persist contact with an administrator.");
            QMessageBox::critical( this , ApplicationNameString , message );
            break;
        case QExecuteOperationThread::ErrorRetrieving :
            message = tr("Please review the operation list screen, ");
            message += tr("an error ocurred retrieving a study.\n");
            message += tr("\nPACS %1 doesn't respond correctly, be sure that your computer is connected on network and the PACS parameters are correct.").arg(pacs.getAEPacs());
            message += tr("\nIf the problem persist contact with an administrator.");
            QMessageBox::critical( this , ApplicationNameString , message );
            break;
        case QExecuteOperationThread::MoveDestinationAETileUnknown:
            message = tr("Please review the operation list screen, ");
            message += tr("PACS %1 doesn't recognize your computer's AETitle %2 and some studies can't be retrieved.").arg(pacs.getAEPacs(), settings.getAETitleMachine());
            message += tr("\n\nContact with an administrador to register your computer to the PACS.");
            QMessageBox::warning( this , ApplicationNameString , message );
            break;
        case QExecuteOperationThread::NoEnoughSpace :
            message = tr("There is not enough space to retrieve studies, please free space.");
            message += tr("\nAll pending retrieve operations will be cancelled.");
            QMessageBox::warning( this , ApplicationNameString , message );
            break;
        case QExecuteOperationThread::ErrorFreeingSpace :
            message = tr("Please review the operation list screen, ");
            message += tr("an error ocurred freeing space and some operations may have failed.");
            message += tr("\n\nClose all %1 windows and try again."
                         "\nIf the problem persist contact with an administrator.").arg(ApplicationNameString);
            QMessageBox::critical( this , ApplicationNameString , message );
            break;
        case QExecuteOperationThread::DatabaseError :
            message = tr("Please review the operation list screen, ");
            message += tr("a database error ocurred and some operations may have failed.");
            message += tr("\n\nClose all %1 windows and try again."
                         "\nIf the problem persist contact with an administrator.").arg(ApplicationNameString);
            QMessageBox::critical( this , ApplicationNameString , message );
            break;
       case QExecuteOperationThread::PatientInconsistent :
            message = tr("Please review the operation list screen, ");
            message += tr("an error ocurred and some operations may have failed.");
            message += tr("\n%1 has not be capable of read correctly dicom information of the study.").arg(ApplicationNameString);
            message += tr("\n\nThe study may be corrupted, if It is not corrupted please contact with %1 team.").arg(ApplicationNameString);
            QMessageBox::critical( this , ApplicationNameString , message );
            break;
	   case QExecuteOperationThread::MoveRefusedOutOfResources :
			message = tr("Please review the operation list screen, ");
            message += tr("PACS %1 is out of resources and can't process the request for retrieving a study.").arg(pacs.getAEPacs());
            message += tr("\n\nTry later to retrieve the study, if the problem persists please contact with PACS administrator to solve the problem.");
            QMessageBox::critical( this , ApplicationNameString , message );
            break;
       case QExecuteOperationThread::IncomingConnectionsPortPacsInUse :
            message = tr("Port %1 for incoming connections from PACS is already in use by another application.").arg(StarviewerSettings().getLocalPort());
            message += tr("\n\n%1 can't retrieve the studies, all pending retrieve operations will be cancelled.").arg(ApplicationNameString);
            message += tr("\n\nIf there is another %1 window retrieving studies from the PACS please wait until those retrieving has finished and try again.").arg(ApplicationNameString);
            QMessageBox::critical( this , ApplicationNameString , message );
            break;
        default:
            message = tr("Please review the operation list screen, an unknown error has ocurred retrieving a study.");
            message += tr("\n\nClose all %1 windows and try again."
                         "\nIf the problem persist contact with an administrator.").arg(ApplicationNameString);
    }
}

void QueryScreen::showDICOMDIRImporterError(QString studyInstanceUID, DICOMDIRImporter::DICOMDIRImporterError error)
{
    QString message;

    switch (error)
    {
        case DICOMDIRImporter::ErrorOpeningDicomdir :
            message = tr("Trying to import study with UID %1 ").arg(studyInstanceUID);
            message += tr("the dicomdir could not be opened. Be sure that the dicomdir path is correct.\n");
            message += tr("\n\nIf the problem persist contact with an administrator.");
            QMessageBox::critical( this , ApplicationNameString , message );
            break;
        case DICOMDIRImporter::ErrorCopyingFiles :
            message = tr("Some files of study with UID %2 could not be imported. Be sure that you have write permissions on the %1 cache directory.").arg(ApplicationNameString, studyInstanceUID);
            message += tr("\n\nIf the problem persist contact with an administrator.");
            QMessageBox::critical( this , ApplicationNameString , message );
            break;
        case DICOMDIRImporter::NoEnoughSpace :
            message = tr("There is not enough space to retrieve studies, please free space.");
            QMessageBox::warning( this , ApplicationNameString , message );
            break;
        case DICOMDIRImporter::ErrorFreeingSpace :
            message = tr("An error has ocurred freeing space, some studies can't be imported.");
            message += tr("\n\nClose all %1 windows and try again."
                         "\nIf the problem persist contact with an administrator.").arg(ApplicationNameString);
            QMessageBox::critical( this , ApplicationNameString , message );
            break;
        case DICOMDIRImporter::DatabaseError :
            message = tr("A database error has ocurred, some studies can't be imported.");
            message += tr("\n\nClose all %1 windows and try again."
                         "\nIf the problem persist contact with an administrator.").arg(ApplicationNameString);
            QMessageBox::critical( this , ApplicationNameString , message );
            break;
       case DICOMDIRImporter::PatientInconsistent :
            message = tr("The study with UID %2 can't be imported, because %1 has not been capable of read correctly dicom information of the study.").arg(ApplicationNameString, studyInstanceUID);
            message += tr("\n\nThe study may be corrupted, if It is not corrupted please contact with %1 team.").arg(ApplicationNameString);
            QMessageBox::critical( this , ApplicationNameString , message );
            break;
       case DICOMDIRImporter::DicomdirInconsistent :
            message = tr("Trying to import study with UID %1 ").arg(studyInstanceUID);
            message += tr("has ocurred an error. This dicomdir is inconsistent, can't be imported.");
            message += tr("\n\nPlease contact with %1 team.").arg(ApplicationNameString);
            QMessageBox::critical( this , ApplicationNameString , message );
            break;
      case DICOMDIRImporter::Ok :
            break;
        default:
            message = tr("An unknown error has ocurred importing dicomdir.");
            message += tr("\n\nClose all %1 windows and try again."
                         "\nIf the problem persist contact with an administrator.").arg(ApplicationNameString);
            QMessageBox::critical( this , ApplicationNameString , message );
    }
}

void QueryScreen::showListenRISRequestThreadError(ListenRISRequestThread::ListenRISRequestThreadError error)
{
    QString message;
    StarviewerSettings settings;

    switch(error)
    {
        case ListenRISRequestThread::risPortInUse :
            message = tr("Can't listen RIS requests on port %1, the port is in use by another application.").arg(settings.getListenPortRisRequests());
            message += tr("\n\nIf the error has ocurred when openned new %1's windows, close this window. To open new %1 window you have to choose the 'New' option from the File menu.").arg(ApplicationNameString);
            break;
        case ListenRISRequestThread::unknowNetworkError :
            message = tr("Can't listen RIS requests on port %1, an unknown network error has produced.").arg(settings.getListenPortRisRequests());
            message += tr("\nIf the problem persist contact with an administrator.");
            break;
    }
    
    QMessageBox::critical(this, ApplicationNameString, message);
}

};

