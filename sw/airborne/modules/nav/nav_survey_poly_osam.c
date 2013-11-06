/*
 * Copyright (C) 2008-2013 The Paparazzi Team
 *
 * This file is part of paparazzi.
 *
 * paparazzi is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * paparazzi is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with paparazzi; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * @file modules/nav/nav_survey_poly_osam.c
 *
 */

#include "modules/nav/nav_survey_poly_osam.h"

#include "subsystems/nav.h"
#include "state.h"
#include "autopilot.h"
#include "generated/flight_plan.h"


struct Point2D {float x; float y;};
struct Line {float m;float b;float x;};

static void TranslateAndRotateFromWorld(struct Point2D *p, float Zrot, float transX, float transY);
static void RotateAndTranslateToWorld(struct Point2D *p, float Zrot, float transX, float transY);
static void FindInterceptOfTwoLines(float *x, float *y, struct Line L1, struct Line L2);
static float EvaluateLineForY(float x, struct Line L);
static float EvaluateLineForX(float y, struct Line L);
static float DistanceEquation(struct Point2D p1,struct Point2D p2);

#define PolygonSize 10
#define MaxFloat   1000000000
#define MinFloat   -1000000000

#ifndef LINE_START_FUNCTION
#define LINE_START_FUNCTION {}
#endif
#ifndef LINE_STOP_FUNCTION
#define LINE_STOP_FUNCTION {}
#endif

/************** Polygon Survey **********************************************/

/** This routine will cover the enitre area of any Polygon defined in the flightplan which is a convex polygon.
 */

enum SurveyStatus { Init, Entry, Sweep, SweepCircle };
static enum SurveyStatus CSurveyStatus;
static struct Point2D SmallestCorner;
static struct Line Edges[PolygonSize];
static float EdgeMaxY[PolygonSize];
static float EdgeMinY[PolygonSize];
static float SurveyTheta;
static float dSweep;
static float SurveyRadius;
static struct Point2D SurveyToWP;
static struct Point2D SurveyFromWP;
static struct Point2D SurveyCircle;
static uint8_t SurveyEntryWP;
static uint8_t SurveySize;
static float SurveyCircleQdr;
static float MaxY;
uint16_t PolySurveySweepNum;
uint16_t PolySurveySweepBackNum;

bool_t nav_survey_poly_osam_start(uint8_t EntryWP, uint8_t Size, float sw, float Orientation)
{
  SmallestCorner.x = 0;
  SmallestCorner.y = 0;
  int i = 0;
  float ys = 0;
  static struct Point2D EntryPoint;
  float LeftYInt;
  float RightYInt;
  float temp;
  float XIntercept1 = 0;
  float XIntercept2 = 0;

  SurveyTheta = RadOfDeg(Orientation);
  PolySurveySweepNum = 0;
  PolySurveySweepBackNum = 0;

  SurveyEntryWP = EntryWP;
  SurveySize = Size;

  struct Point2D Corners[PolygonSize];

  CSurveyStatus = Init;

  if (Size == 0)
    return TRUE;

  //Don't initialize if Polygon is too big or if the orientation is not between 0 and 90
  if(Size <= PolygonSize && Orientation >= -90 && Orientation <= 90)
  {
    //Initialize Corners
    for(i = 0; i < Size; i++)
    {
      Corners[i].x = waypoints[i+EntryWP].x;
      Corners[i].y = waypoints[i+EntryWP].y;
    }

    //Rotate Corners so sweeps are parellel with x axis
    for(i = 0; i < Size; i++)
      TranslateAndRotateFromWorld(&Corners[i], SurveyTheta, 0, 0);

    //Find min x and min y
    SmallestCorner.y = Corners[0].y;
    SmallestCorner.x = Corners[0].x;
    for(i = 1; i < Size; i++)
    {
      if(Corners[i].y < SmallestCorner.y)
        SmallestCorner.y = Corners[i].y;

      if(Corners[i].x < SmallestCorner.x)
        SmallestCorner.x = Corners[i].x;
    }

    //Translate Corners all exist in quad #1
    for(i = 0; i < Size; i++)
      TranslateAndRotateFromWorld(&Corners[i], 0, SmallestCorner.x, SmallestCorner.y);

    //Rotate and Translate Entry Point
    EntryPoint.x = Corners[0].x;
    EntryPoint.y = Corners[0].y;

    //Find max y
    MaxY = Corners[0].y;
    for(i = 1; i < Size; i++)
    {
      if(Corners[i].y > MaxY)
        MaxY = Corners[i].y;
    }

    //Find polygon edges
    for(i = 0; i < Size; i++)
    {
      if(i == 0)
        if(Corners[Size-1].x == Corners[i].x) //Don't divide by zero!
          Edges[i].m = MaxFloat;
        else
          Edges[i].m = ((Corners[Size-1].y-Corners[i].y)/(Corners[Size-1].x-Corners[i].x));
      else
        if(Corners[i].x == Corners[i-1].x)
          Edges[i].m = MaxFloat;
        else
          Edges[i].m = ((Corners[i].y-Corners[i-1].y)/(Corners[i].x-Corners[i-1].x));

      //Edges[i].m = MaxFloat;
      Edges[i].b = (Corners[i].y - (Corners[i].x*Edges[i].m));
    }

    //Find Min and Max y for each line
    FindInterceptOfTwoLines(&temp, &LeftYInt, Edges[0], Edges[1]);
    FindInterceptOfTwoLines(&temp, &RightYInt, Edges[0], Edges[Size-1]);

    if(LeftYInt > RightYInt)
    {
      EdgeMaxY[0] = LeftYInt;
      EdgeMinY[0] = RightYInt;
    }
    else
    {
      EdgeMaxY[0] = RightYInt;
      EdgeMinY[0] = LeftYInt;
    }

    for(i = 1; i < Size-1; i++)
    {
      FindInterceptOfTwoLines(&temp, &LeftYInt, Edges[i], Edges[i+1]);
      FindInterceptOfTwoLines(&temp, &RightYInt, Edges[i], Edges[i-1]);

      if(LeftYInt > RightYInt)
      {
        EdgeMaxY[i] = LeftYInt;
        EdgeMinY[i] = RightYInt;
      }
      else
      {
        EdgeMaxY[i] = RightYInt;
        EdgeMinY[i] = LeftYInt;
      }
    }

    FindInterceptOfTwoLines(&temp, &LeftYInt, Edges[Size-1], Edges[0]);
    FindInterceptOfTwoLines(&temp, &RightYInt, Edges[Size-1], Edges[Size-2]);

    if(LeftYInt > RightYInt)
    {
      EdgeMaxY[Size-1] = LeftYInt;
      EdgeMinY[Size-1] = RightYInt;
    }
    else
    {
      EdgeMaxY[Size-1] = RightYInt;
      EdgeMinY[Size-1] = LeftYInt;
    }

    //Find amount to increment by every sweep
    if(EntryPoint.y >= MaxY/2)
      dSweep = -sw;
    else
      dSweep = sw;

    //CircleQdr tells the plane when to exit the circle
    if(dSweep >= 0)
      SurveyCircleQdr = -DegOfRad(SurveyTheta);
    else
      SurveyCircleQdr = 180-DegOfRad(SurveyTheta);

    //Find y value of the first sweep
    ys = EntryPoint.y+(dSweep/2);

    //Find the edges which intercet the sweep line first
    for(i = 0; i < SurveySize; i++)
    {
      if(EdgeMinY[i] <= ys && EdgeMaxY[i] > ys)
      {
        XIntercept2 = XIntercept1;
        XIntercept1 = EvaluateLineForX(ys, Edges[i]);
      }
    }

    //Find point to come from and point to go to
    if(fabs(EntryPoint.x - XIntercept2) <= fabs(EntryPoint.x - XIntercept1))
    {
      SurveyToWP.x = XIntercept1;
      SurveyToWP.y = ys;

      SurveyFromWP.x = XIntercept2;
      SurveyFromWP.y = ys;
    }
    else
    {
      SurveyToWP.x = XIntercept2;
      SurveyToWP.y = ys;

      SurveyFromWP.x = XIntercept1;
      SurveyFromWP.y = ys;
    }

    //Find the direction to circle
    if(ys > 0 && SurveyToWP.x > SurveyFromWP.x)
      SurveyRadius = dSweep/2;
    else if(ys < 0 && SurveyToWP.x < SurveyFromWP.x)
      SurveyRadius = dSweep/2;
    else
      SurveyRadius = -dSweep/2;

    //Find the entry circle
    SurveyCircle.x = SurveyFromWP.x;
    SurveyCircle.y = EntryPoint.y;

    //Go into entry circle state
    CSurveyStatus = Entry;
    LINE_STOP_FUNCTION;
  }

  return FALSE;
}

bool_t nav_survey_poly_osam_run(void)
{
  struct Point2D C;
  struct Point2D ToP;
  struct Point2D FromP;
  float ys;
  static struct Point2D LastPoint;
  int i;
  bool_t SweepingBack = FALSE;
  float XIntercept1 = 0;
  float XIntercept2 = 0;
  float DInt1 = 0;
  float DInt2 = 0;

  NavVerticalAutoThrottleMode(0); /* No pitch */
  NavVerticalAltitudeMode(waypoints[SurveyEntryWP].a, 0.);

  switch(CSurveyStatus)
  {
  case Entry:
    //Rotate and translate circle point into real world
    C.x = SurveyCircle.x;
    C.y = SurveyCircle.y;
    RotateAndTranslateToWorld(&C, 0, SmallestCorner.x, SmallestCorner.y);
    RotateAndTranslateToWorld(&C, SurveyTheta, 0, 0);

    //follow the circle
    nav_circle_XY(C.x, C.y, SurveyRadius);

    if(NavQdrCloseTo(SurveyCircleQdr) && NavCircleCountNoRewind() > .1 && stateGetPositionEnu_f()->z > waypoints[SurveyEntryWP].a-10)
    {
      CSurveyStatus = Sweep;
      nav_init_stage();
      LINE_START_FUNCTION;
    }
    break;
  case Sweep:
    //Rotate and Translate Line points into real world
    ToP.x = SurveyToWP.x;
    ToP.y = SurveyToWP.y;
    FromP.x = SurveyFromWP.x;
    FromP.y = SurveyFromWP.y;

    RotateAndTranslateToWorld(&ToP, 0, SmallestCorner.x, SmallestCorner.y);
    RotateAndTranslateToWorld(&ToP, SurveyTheta, 0, 0);

    RotateAndTranslateToWorld(&FromP, 0, SmallestCorner.x, SmallestCorner.y);
    RotateAndTranslateToWorld(&FromP, SurveyTheta, 0, 0);

    //follow the line
    nav_route_xy(FromP.x,FromP.y,ToP.x,ToP.y);
    if(nav_approaching_xy(ToP.x,ToP.y,FromP.x,FromP.y, 0))
    {
      LastPoint.x = SurveyToWP.x;
      LastPoint.y = SurveyToWP.y;

      if(LastPoint.y+dSweep >= MaxY || LastPoint.y+dSweep <= 0) //Your out of the Polygon so Sweep Back
      {
        dSweep = -dSweep;
        ys = LastPoint.y+(dSweep/2);

        if(dSweep >= 0)
          SurveyCircleQdr = -DegOfRad(SurveyTheta);
        else
          SurveyCircleQdr = 180-DegOfRad(SurveyTheta);
        SweepingBack = TRUE;
        PolySurveySweepBackNum++;
      }
      else
      {
        //Find y value of the first sweep
        ys = LastPoint.y+dSweep;
      }

      //Find the edges which intercet the sweep line first
      for(i = 0; i < SurveySize; i++)
      {
        if(EdgeMinY[i] < ys && EdgeMaxY[i] >= ys)
        {
          XIntercept2 = XIntercept1;
          XIntercept1 = EvaluateLineForX(ys, Edges[i]);
        }
      }

      //Find point to come from and point to go to
      DInt1 = XIntercept1 -  LastPoint.x;
      DInt2 = XIntercept2 - LastPoint.x;

      if(DInt1 * DInt2 >= 0)
      {
        if(fabs(DInt2) <= fabs(DInt1))
        {
          SurveyToWP.x = XIntercept1;
          SurveyToWP.y = ys;

          SurveyFromWP.x = XIntercept2;
          SurveyFromWP.y = ys;
        }
        else
        {
          SurveyToWP.x = XIntercept2;
          SurveyToWP.y = ys;

          SurveyFromWP.x = XIntercept1;
          SurveyFromWP.y = ys;
        }
      }
      else
      {
        if((SurveyToWP.x - SurveyFromWP.x) > 0 && DInt2 > 0)
        {
          SurveyToWP.x = XIntercept1;
          SurveyToWP.y = ys;

          SurveyFromWP.x = XIntercept2;
          SurveyFromWP.y = ys;
        }
        else if((SurveyToWP.x - SurveyFromWP.x) < 0 && DInt2 < 0)
        {
          SurveyToWP.x = XIntercept1;
          SurveyToWP.y = ys;

          SurveyFromWP.x = XIntercept2;
          SurveyFromWP.y = ys;
        }
        else
        {
          SurveyToWP.x = XIntercept2;
          SurveyToWP.y = ys;

          SurveyFromWP.x = XIntercept1;
          SurveyFromWP.y = ys;
        }
      }



      if(fabs(LastPoint.x-SurveyToWP.x) > fabs(SurveyFromWP.x-SurveyToWP.x))
        SurveyCircle.x = LastPoint.x;
      else
        SurveyCircle.x = SurveyFromWP.x;


      if(!SweepingBack)
        SurveyCircle.y = LastPoint.y+(dSweep/2);
      else
        SurveyCircle.y = LastPoint.y;

      //Find the direction to circle
      if(ys > 0 && SurveyToWP.x > SurveyFromWP.x)
        SurveyRadius = dSweep/2;
      else if(ys < 0 && SurveyToWP.x < SurveyFromWP.x)
        SurveyRadius = dSweep/2;
      else
        SurveyRadius = -dSweep/2;

      //Go into circle state
      CSurveyStatus = SweepCircle;
      nav_init_stage();
      LINE_STOP_FUNCTION;
      PolySurveySweepNum++;
    }

    break;
  case SweepCircle:
    //Rotate and translate circle point into real world
    C.x = SurveyCircle.x;
    C.y = SurveyCircle.y;
    RotateAndTranslateToWorld(&C, 0, SmallestCorner.x, SmallestCorner.y);
    RotateAndTranslateToWorld(&C, SurveyTheta, 0, 0);

    //follow the circle
    nav_circle_XY(C.x, C.y, SurveyRadius);

    if(NavQdrCloseTo(SurveyCircleQdr) && NavCircleCount() > 0)
    {
      CSurveyStatus = Sweep;
      nav_init_stage();
      LINE_START_FUNCTION;
    }
    break;
  case Init:
    return FALSE;
  default:
    return FALSE;
  }
  return TRUE;
}


/*
  Translates point so (transX, transY) are (0,0) then rotates the point around z by Zrot
*/
void TranslateAndRotateFromWorld(struct Point2D *p, float Zrot, float transX, float transY)
{
  float temp;

  p->x = p->x - transX;
  p->y = p->y - transY;

  temp = p->x;
  p->x = p->x*cosf(Zrot)+p->y*sinf(Zrot);
  p->y = -temp*sinf(Zrot)+p->y*cosf(Zrot);
}

/// Rotates point round z by -Zrot then translates so (0,0) becomes (transX,transY)
void RotateAndTranslateToWorld(struct Point2D *p, float Zrot, float transX, float transY)
{
  float temp = p->x;

  p->x = p->x*cosf(Zrot)-p->y*sinf(Zrot);
  p->y = temp*sinf(Zrot)+p->y*cosf(Zrot);

  p->x = p->x + transX;
  p->y = p->y + transY;
}

void FindInterceptOfTwoLines(float *x, float *y, struct Line L1, struct Line L2)
{
  *x = ((L2.b-L1.b)/(L1.m-L2.m));
  *y = L1.m*(*x)+L1.b;
}

float EvaluateLineForY(float x, struct Line L)
{
  return (L.m*x)+L.b;
}

float EvaluateLineForX(float y, struct Line L)
{
  return ((y-L.b)/L.m);
}

float DistanceEquation(struct Point2D p1,struct Point2D p2)
{
  return sqrtf((p1.x-p2.x)*(p1.x-p2.x)+(p1.y-p2.y)*(p1.y-p2.y));
}
