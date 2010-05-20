/***************************************************************************
 *   Copyright (C) 2005-2007 by Grup de Gràfics de Girona                  *
 *   http://iiia.udg.es/GGG/index.html?langu=uk                            *
 *                                                                         *
 *   Universitat de Girona                                                 *
 ***************************************************************************/
#include "nonclosedangletool.h"
#include "q2dviewer.h"
#include "logging.h"
#include "drawer.h"
#include "drawerline.h"
#include "drawertext.h"
#include "mathtools.h"
//vtk
#include <vtkRenderWindowInteractor.h>
#include <vtkCommand.h>

namespace udg {

NonClosedAngleTool::NonClosedAngleTool( QViewer *viewer, QObject *parent )
 : Tool(viewer, parent), m_firstLine(0), m_secondLine(0), m_state(None), m_lineState(NoPoints)
{
    m_toolName = "NonClosedAngleTool";
    m_hasSharedData = false;

    m_2DViewer = qobject_cast<Q2DViewer *>( viewer );
    if( !m_2DViewer )
        DEBUG_LOG(QString("El casting no ha funcionat!!! És possible que viewer no sigui un Q2DViewer!!!-> ")+ viewer->metaObject()->className() );

    connect( m_2DViewer, SIGNAL( volumeChanged(Volume *) ), SLOT( initialize() ) );
}

NonClosedAngleTool::~NonClosedAngleTool()
{
    bool hasToRefresh = false;
    // Cal decrementar el reference count perquè 
    // l'annotació s'esborri si "matem" l'eina
    if ( m_firstLine )
    {
        m_firstLine->decreaseReferenceCount();
        delete m_firstLine;
        hasToRefresh = true;
    }
    
    if ( m_secondLine )
    {
        m_secondLine->decreaseReferenceCount();
        delete m_secondLine;
        hasToRefresh = true;
    }

    if( hasToRefresh )
        m_2DViewer->render();
}

void NonClosedAngleTool::handleEvent( long unsigned eventID )
{
    switch( eventID )
    {
        case vtkCommand::LeftButtonPressEvent:

            if( m_2DViewer->getInput() )
            {
                if ( m_2DViewer->getInteractor()->GetRepeatCount() == 0 )
                {
                    this->annotateLinePoints();

                    if ( m_state == SecondLineFixed )
                    {
                        computeAngle();
                        // Així alliberem les primitives perquè puguin ser esborrades
                        m_firstLine->decreaseReferenceCount();
                        m_secondLine->decreaseReferenceCount();
                        m_middleLine->decreaseReferenceCount();
                        //Acabem les línies
                        m_firstLine = NULL;
                        m_secondLine = NULL;
                        m_middleLine = NULL;

                        //Restaurem m_state
                        m_state = None;
                    }

                }
            }
        break;

        case vtkCommand::MouseMoveEvent:

            if( m_firstLine && m_state == None )
                this->simulateLine(m_firstLine);
            else if ( m_secondLine && m_state == FirstLineFixed )
                this->simulateLine(m_secondLine);

            m_2DViewer->render();

        break;
    }
}

void NonClosedAngleTool::annotateLinePoints()
{
    DrawerLine *line;

    //creem primera o segona línies
    if ( ( m_state == None && m_lineState == NoPoints ) ||
        ( m_state == FirstLineFixed && m_lineState == NoPoints ) )
    {
        line = new DrawerLine;
        // Així evitem que la primitiva pugui ser esborrada durant l'edició per events externs
        line->increaseReferenceCount();
    }
    else if ( m_state == None )
        line = m_firstLine;
    else
        line = m_secondLine;

    double clickedWorldPoint[3];
    m_2DViewer->getEventWorldCoordinate( clickedWorldPoint );

    //afegim el punt
    if( m_lineState == NoPoints )
    {
        line->setFirstPoint( clickedWorldPoint );
        line->setSecondPoint( clickedWorldPoint );
        m_lineState = FirstPoint;

        if ( m_state == None )
            m_firstLine = line;
        else
            m_secondLine = line;

        m_2DViewer->getDrawer()->draw( line , m_2DViewer->getView(), m_2DViewer->getCurrentSlice() );
    }
    else
    {
        line->setSecondPoint( clickedWorldPoint );

        line->update( DrawerPrimitive::VTKRepresentation );

        m_lineState = NoPoints;

        if ( m_state == None )
            m_state = FirstLineFixed;
        else
            m_state = SecondLineFixed;
    }
}

void NonClosedAngleTool::simulateLine(DrawerLine *line)
{
    double clickedWorldPoint[3];
    m_2DViewer->getEventWorldCoordinate( clickedWorldPoint );
    line->setSecondPoint( clickedWorldPoint );
    //Actualitzem viewer
    line->update( DrawerPrimitive::VTKRepresentation );
}

void NonClosedAngleTool::computeAngle()
{
    if ( !m_middleLine )
    {
        m_middleLine = new DrawerLine;
        // Així evitem que la primitiva pugui ser esborrada durant l'edició per events externs
        m_middleLine->increaseReferenceCount();
    }
    m_middleLine->setLinePattern(DrawerPrimitive::DiscontinuousLinePattern);

    double *p1 = m_firstLine->getFirstPoint();
    double *p2 = m_firstLine->getSecondPoint();
    double *p3 = m_secondLine->getFirstPoint();
    double *p4 = m_secondLine->getSecondPoint();

    double *vd1, *vd2;

    double *intersection;
    int state;

    intersection = MathTools::infiniteLinesIntersection(p1,p2,p3,p4,state);

    double dist1, dist2, dist3, dist4;
    dist1 = MathTools::getDistance3D(intersection, p1);
    dist2 = MathTools::getDistance3D(intersection, p2);
    dist3 = MathTools::getDistance3D(intersection, p3);
    dist4 = MathTools::getDistance3D(intersection, p4);

    // Per calcular el vectors directors farem servir la intersecció i el punt 
    // més llunyà a la intersecció de cada recta ja que si per alguna casualitat
    // l'usuari fa coincidir un dels punts de cada recta, la distància seria de 0 
    // i com a conseqüència l'angle calculat ens sortiria Nan 
    if ( dist1 <= dist2 )
    {
        if ( dist3 <= dist4 )
        {
            vd1 = MathTools::directorVector( p2, intersection );
            vd2 = MathTools::directorVector( p4, intersection );
            m_middleLine->setFirstPoint(p1);
            m_middleLine->setSecondPoint(p3);
        }
        else
        {
            vd1 = MathTools::directorVector( p2, intersection );
            vd2 = MathTools::directorVector( p3, intersection );
            m_middleLine->setFirstPoint(p1);
            m_middleLine->setSecondPoint(p4);
        }
    }
    else
    {
        if ( dist3 <= dist4 )
        {
            vd1 = MathTools::directorVector( p1, intersection );
            vd2 = MathTools::directorVector( p4, intersection );
            m_middleLine->setFirstPoint(p2);
            m_middleLine->setSecondPoint(p3);
        }
        else
        {
            vd1 = MathTools::directorVector( p1, intersection );
            vd2 = MathTools::directorVector( p3, intersection );
            m_middleLine->setFirstPoint(p2);
            m_middleLine->setSecondPoint(p4);
        }
    }

    //dibuixem la línia auxiliar
    m_2DViewer->getDrawer()->draw( m_middleLine , m_2DViewer->getView(), m_2DViewer->getCurrentSlice() );

    for (int i = 0; i < 3; i++)
    {
        if ( fabs( vd1[i] ) < 0.0001 )
            vd1[i] = 0.0;

        if ( fabs( vd2[i] ) < 0.0001 )
            vd2[i] = 0.0;
    }

    double angle = MathTools::angleInDegrees( vd1, vd2 );

    DrawerText * text = new DrawerText;

    if ( state == MathTools::ParallelLines )
        text->setText( tr("0.0 degrees") );
    else if ( state == MathTools::SkewIntersection )   //Won't occur
        text->setText( tr("Skew lines.") );
    else
        text->setText( tr("%1 degrees").arg( angle,0,'f',1) );


    textPosition( m_middleLine->getFirstPoint(), m_middleLine->getSecondPoint(), text );

    text->update( DrawerPrimitive::VTKRepresentation );
    text->shadowOn();
    m_2DViewer->getDrawer()->draw( text , m_2DViewer->getView(), m_2DViewer->getCurrentSlice() );
}

void NonClosedAngleTool::textPosition( double *p1, double *p2, DrawerText *angleText )
{
    double position[3];
    int horizontalCoord, verticalCoord, zCoordinate;

    switch( m_2DViewer->getView() )
    {
        case Q2DViewer::Axial:
            horizontalCoord = 0;
            verticalCoord = 1;
            zCoordinate = 2;
            break;

        case Q2DViewer::Sagital:
            horizontalCoord = 1;
            verticalCoord = 2;
            zCoordinate = 0;
            break;

        case Q2DViewer::Coronal:
            horizontalCoord = 0;
            verticalCoord = 2;
            zCoordinate = 1;
            break;
    }

    position[horizontalCoord] = ( p1[horizontalCoord] + p2[horizontalCoord] ) / 2.0;
    position[verticalCoord] = ( p1[verticalCoord] + p2[verticalCoord] ) / 2.0;
    position[zCoordinate] = p1[zCoordinate];

    angleText->setAttachmentPoint(position);

}

void NonClosedAngleTool::initialize()
{
    // Alliberem les primitives perquè puguin ser esborrades
    if( m_firstLine )
    {
        m_firstLine->decreaseReferenceCount();
        delete m_firstLine;
    }
    
    if( m_secondLine )
    {
        m_secondLine->decreaseReferenceCount();
        delete m_secondLine;
    }
    
    m_firstLine = NULL;
    m_secondLine = NULL;
    m_state = None;
    m_lineState = NoPoints;
}

}
