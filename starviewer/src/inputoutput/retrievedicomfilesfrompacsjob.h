/***************************************************************************
 *   Copyright (C) 2005 by Grup de Gr�fics de Girona                       *
 *   http://iiia.udg.es/GGG/index.html?langu=uk                            *
 *                                                                         *
 *   Universitat de Girona                                                 *
 ***************************************************************************/
#ifndef UDGRETRIEVEDICOMFILESFROMPACS_H
#define UDGRETRIEVEDICOMFILESFROMPACS_H

#include <QObject>

#include "pacsjob.h"
#include "pacsrequeststatus.h"
#include "dicommask.h"

namespace udg {

/**
    Job que s'encarrega de descarregar fitxers del PACS.

   @author Grup de Gr�fics de Girona  ( GGG ) <vismed@ima.udg.es>
*/
class Study;
class RetrieveDICOMFilesFromPACS;
class PacsDevice;
class LocalDatabaseManagerThreaded;
class PatientFiller;
class QThreadRunWithExec;
class LocalDatabaseManager;
class DICOMTagReader;

class RetrieveDICOMFilesFromPACSJob: public PACSJob
{
Q_OBJECT
public:
    ///Definim la prioritat del Job, aquesta prioritat ens definir� l'ordre d'execuci� en la cua
    enum RetrievePriorityJob { Low = 0, Medium = 2, High = 3 };
 
    ///Constructor/Destructor de la classe
    RetrieveDICOMFilesFromPACSJob(PacsDevice pacsDevice, Study *studyToRetrieveDICOMFiles, DicomMask dicomMaskToRetrieve, RetrievePriorityJob);
    ~RetrieveDICOMFilesFromPACSJob();

    ///Retorna el tipus de PACSJob que �s l'objecte
    PACSJob::PACSJobType getPACSJobType();

    ///Retorna l'estudi del qual s'han de descarregar els fitxers
    Study* getStudyToRetrieveDICOMFiles();

    ///Codi que executar� el job
    void run();

    ///Sol�licita abortar el job
    void requestAbort();

    ///Retorna l'Status del Job
    PACSRequestStatus::RetrieveRequestStatus getStatus();

    /**Retorna l'Status descrit en un QString , aquest QString est� pensat per ser mostrat en QMessageBox per informar a l'usuari de l'estat que ha retornat
        el job en el mateixa descripci� s'indica de quin �s l'estudi afectat*/
    QString getStatusDescription();

signals:

    ///Signal que s'emet quan s'ha descarregat un fitxer
    void DICOMFileRetrieved(PACSJob *pacsJob, int numberOfImagesRetrieved);

    ///Signal que s'emet quan s'ha descarregat una s�rie
    void DICOMSeriesRetrieved(PACSJob *pacsJob, int numberOfSeriesRetrieved);

private slots:

    ///Slot que s'activa quan s'ha descarregat una imatge, resp�n al signal DICOMFileRetrieved de RetrieveDICOMFilesFromPACS
    void DICOMFileRetrieved(DICOMTagReader *dicomTagReader, int numberOfImagesRetrieved);

private:

    ///Indica la prioritat del job
    //Sobreescribim el m�tode priority de la classe ThreadWeaver::Job
    int priority() const;

    ///M�tode que ens indica si hi ha espai disponible per descarregar estudis
    PACSRequestStatus::RetrieveRequestStatus thereIsAvailableSpaceOnHardDisk();

    ///Indica si l'estudi amb l'UID passat per�metre ja existeix a la base de dades
    bool existStudyInLocalDatabase(QString studyInstanceUID);

    ///Crea les connexions entre els diversos elements per guardar els fitxers descarregat a la Base de dades
    void createRetrieveDICOMFilesConnections(LocalDatabaseManager *localDatabaseManager,LocalDatabaseManagerThreaded *localDatabaseManagerThreaded, PatientFiller *patientFiller, QThreadRunWithExec *fillersThread);

    ///Esborra els fitxers descarregats de la cach� si l'estudi no existeix a la base de dades
    /*Aquest m�tode est� pensat en casos que la desc�rrega falla i volem esborrar els fitxers descarregats, nom�s s'esborran si l'estudi no est� inserit
      a la bd, si l'estudi est� inserit no l'esborrem, perqu� part dels fitxers descarregats ja podien estar inserit a la base de dades per una anterior
      desc�rrega*/
    void deleteRetrievedDICOMFilesIfStudyNotExistInDatabase();

private:

    bool m_abortRequested;
    RetrieveDICOMFilesFromPACS *m_retrieveDICOMFilesFromPACS;
    Study *m_studyToRetrieveDICOMFiles;
    DicomMask m_dicomMaskToRetrieve;
    PACSRequestStatus::RetrieveRequestStatus m_retrieveRequestStatus;
    RetrievePriorityJob m_retrievePriorityJob;
    QList<QString> m_retrievedSeriesInstanceUID;
    QString m_lastImageSeriesInstanceUID;
    int m_numberOfSeriesRetrieved;
    PatientFiller *m_patientFiller;
};

};

#endif
