/*************************************************************************************
  Copyright (C) 2014 Laboratori de Gràfics i Imatge, Universitat de Girona &
  Institut de Diagnòstic per la Imatge.
  Girona 2014. All rights reserved.
  http://starviewer.udg.edu

  This file is part of the Starviewer (Medical Imaging Software) open source project.
  It is subject to the license terms in the LICENSE file found in the top-level
  directory of this distribution and at http://starviewer.udg.edu/license. No part of
  the Starviewer (Medical Imaging Software) open source project, including this file,
  may be copied, modified, propagated, or distributed except according to the
  terms contained in the LICENSE file.
 *************************************************************************************/

#include "volumereadermanager.h"

#include "asynchronousvolumereader.h"
#include "volumereaderjob.h"
#include "volume.h"

namespace udg {

VolumeReaderManager::VolumeReaderManager(QObject *parent) :
    QObject(parent)
{
}

VolumeReaderManager::~VolumeReaderManager()
{
    cancelReading();
}

void VolumeReaderManager::initialize()
{
    m_volumeReaderJobs.clear();
    m_jobsProgress.clear();
    m_volumes.clear();
    m_success = true;
    m_lastError = "";
    m_numberOfFinishedJobs = 0;
}

void VolumeReaderManager::readVolume(Volume *volume)
{
    QList<Volume*> volumes;
    volumes << volume;

    readVolumes(volumes);
}

void VolumeReaderManager::readVolumes(const QList<Volume*> &volumes)
{
    initialize();

    foreach (Volume *volume, volumes)
    {
        // TODO Esborrar volumeReader!!
        AsynchronousVolumeReader *volumeReader = new AsynchronousVolumeReader();
        m_volumeReaderJobs << volumeReader->read(volume);
        m_jobsProgress.insert(m_volumeReaderJobs.last().dynamicCast<VolumeReaderJob>().data(), 0);
        m_volumes << NULL;
        connect(m_volumeReaderJobs.last().dynamicCast<VolumeReaderJob>().data(), SIGNAL(done(ThreadWeaver::JobPointer)), SLOT(jobFinished(ThreadWeaver::JobPointer)));
        connect(m_volumeReaderJobs.last().dynamicCast<VolumeReaderJob>().data(), SIGNAL(progress(VolumeReaderJob*, int)), SLOT(updateProgress(VolumeReaderJob*, int)));
    }
}

void VolumeReaderManager::cancelReading()
{
    // TODO: Aquí s'hauria de cancel·lar realment el current job. De moment no podem fer-ho i simplement el desconnectem
    // Quan es faci bé, tenir en compte què passa si algun altre visor el vol continuar descarregant igualment i nosaltres aquí el cancelem?
    for (int i = 0; i < m_volumeReaderJobs.size(); ++i)
    {
        if (!m_volumeReaderJobs[i].isNull())
        {
            disconnect(m_volumeReaderJobs[i].dynamicCast<VolumeReaderJob>().data(), SIGNAL(done(ThreadWeaver::JobPointer)), this, SLOT(jobFinished(ThreadWeaver::JobPointer)));
            disconnect(m_volumeReaderJobs[i].dynamicCast<VolumeReaderJob>().data(), SIGNAL(progress(VolumeReaderJob*, int)), this, SLOT(updateProgress(VolumeReaderJob*, int)));
        }
        m_volumeReaderJobs[i].clear();
    }
    initialize();
}

bool VolumeReaderManager::readingSuccess()
{
    return m_success;
}

Volume* VolumeReaderManager::getVolume()
{
    return m_volumes.first();
}

QList<Volume*> VolumeReaderManager::getVolumes()
{
    return m_volumes;
}

QString VolumeReaderManager::getLastErrorMessageToUser()
{
    // TODO Ara només retorna el missatge del primer que ha fallat
    return m_lastError;
}

bool VolumeReaderManager::isReading()
{
    return m_numberOfFinishedJobs < m_volumeReaderJobs.size();
}

void VolumeReaderManager::updateProgress(VolumeReaderJob *job, int value)
{
    m_jobsProgress.insert(job, value);

    int currentProgress = 0;
    foreach (int volumeProgressValue, m_jobsProgress)
    {
        currentProgress += volumeProgressValue;
    }

    currentProgress /= m_jobsProgress.size();

    emit progress(currentProgress);
}

void VolumeReaderManager::jobFinished(ThreadWeaver::JobPointer job)
{
    VolumeReaderJob *volumeReaderJob = job.dynamicCast<VolumeReaderJob>().data();
    m_volumes[m_volumeReaderJobs.indexOf(job)] = volumeReaderJob->getVolume();

    if (m_success)
    {
        m_success = job->success();

        if (!m_success)
        {
            m_lastError = volumeReaderJob->getLastErrorMessageToUser();
        }
    }

    m_numberOfFinishedJobs++;

    if (!isReading())
    {
        emit readingFinished();
    }
}

} // namespace udg
