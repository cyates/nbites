// This file is part of Man, a robotic perception, locomotion, and
// team strategy application created by the Northern Bites RoboCup
// team of Bowdoin College in Brunswick, Maine, for the Aldebaran
// Nao robot.
//
// Man is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Man is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
// GNU Lesser Public License for more details.
//
// You should have received a copy of the GNU General Public License
// and the GNU Lesser Public License along with Man.  If not, see
// <http://www.gnu.org/licenses/>.


/*
 * Context - holds context information on field objects in order to
 * better recognize them and identify them.
 */

#include <iostream>
#include "Context.h"
#include "debug.h"
#include "FieldConstants.h"
#include "Utility.h"
#include <vector>
#include <list>
#include <boost/shared_ptr.hpp>

using namespace std;

static const float GOALBOX_FUDGE = 1.5;

Context::Context(Vision *vis, Threshold* thr, Field* fie)
    : vision(vis), thresh(thr), field(fie)
{
    // Initialize the array of VisualFieldObject which we use for distance
    // based identification of corners
    allFieldObjects[0] = vision->bgrp;
    allFieldObjects[1] = vision->bglp;
    allFieldObjects[2] = vision->ygrp;
    allFieldObjects[3] = vision->yglp;
#ifdef OFFLINE
    debugIdentifyCorners = false;
#endif

    init();
}

void Context::init() {
    rightYellowPost = false;
    leftYellowPost = false;
    unknownYellowPost = false;
    yellowPost = false;
    rightBluePost = false;
    leftBluePost = false;
    unknownBluePost = false;
    bluePost = false;
    tCorner = 0;
    lCorner = 0;
    iCorner = 0;
    oCorner = 0;
    cCorner = 0;
    cross = false;
    unknownCross = false;
    yellowCross = false;
    blueCross = false;
    ball = false;
    seeGoalBoxLines = false;
    seeCenterCircle = false;
    sameHalf = false;
    face = FACING_UNKNOWN;
    objectRightX = -1;
}

/**
 * Given a list of VisualCorners, attempts to assign ConcreteCorners (ideally
 * one, but sometimes multiple) that correspond with where the corner could
 * possibly be on the field.  For instance, if we have a T corner and it is
 * right next to the blue goal left post, then it is the blue goal right T.
 * Modifies the corners passed in by calling the setPossibleCorners method;
 * in certain cases the shape of a corner might be switched too (if an L
 * corner is determined to be a T instead, its shape is changed accordingly).
 *
 * Note: 1/11/2010 - This method is still pretty similar to what we used
 * in the past.  It should probably be udpated even more.  I have not gotten
 * rid of some things simply because I'm not sure why they were there.
 * Chown
 *
 * @param corners      The list of all corners found in fieldLines
 */
void Context::identifyCorners(list <VisualCorner> &corners)
{
    if (corners.size() == 0) {
        // nothing to see here - move along
        return;
    }

    // Figure out what goal posts can provide us with contextual cues
    vector <const VisualFieldObject*> visibleObjects = getVisibleFieldObjects();
    setFacing();
    setFieldHalf();
    if (debugIdentifyCorners) {
        printContext();
    }
	int numCorners = corners.size();
    // collect up some information on corners that are connected
	if (numCorners > 1) {
        checkForConnectedCorners(corners);
	} else {
        list <VisualCorner>::iterator one = corners.begin();
        if (one->getShape() == INNER_L) {
            classifyInnerL(*one);
        } else if (one->getShape() == OUTER_L) {
            classifyOuterL(*one);
        } else if (one->getShape() == T) {
            classifyT(*one);
        }
    }

	// We might later use uncertain objects, but they cause problems. e.g. if you
	// see one post as 2 posts (both the left and right), you get really bad things
	// or if one post is badly estimated.
	if (visibleObjects.empty()) {
		visibleObjects = getAllVisibleFieldObjects();
    }
    checkForSidelineInformation(corners);

    // No explicit movement of iterator; will do it manually
    for (list <VisualCorner>::iterator i = corners.begin();i != corners.end();){
        if (debugIdentifyCorners) {
            cout << endl << "Before identification: Corner: "
                 << endl << "\t" << *i << endl;
			cout << "     Shape info: " << i->getSecondaryShape() << endl;
            cout << "     Orientation: " << i->getOrientation() << endl;
        }

        const list <const ConcreteCorner*> possibleClassifications =
            classifyCornerWithObjects(*i, visibleObjects);

        // Keep it completely abstract
		// NOTE: this is dumb - we need to be smarter.  E.g. at least
		// eliminate posts that are WAY off
		// Sometimes the distances fail because the object is occluded
		// so we need another method for this case
		// also, this may be a chance to detect a bad post
        if (possibleClassifications.empty()) {
            i->setPossibleCorners(ConcreteCorner::getPossibleCorners(
                                      i->getShape(), i->getSecondaryShape()));
            if (debugIdentifyCorners) {
                cout << "No matches were found for this corner; going to keep "
                     << "it completely abstract." << endl;
                printPossibilities(i->getPossibleCorners());
            }
            ++i;
        }

        // It is unique, append to the front of the list
        // For localization we want the positively identified corners to come
        // first so  that  they can inform the localization system and help
        // identify abstract corners that might be in the frame
        else if (possibleClassifications.size() == 1) {
            VisualCorner copy = *i;
            copy.setPossibleCorners(possibleClassifications);
            // This has the effect of incrementing our iterator and deleting the
            // corner from our list.
            i = corners.erase(i);
            corners.push_front(copy);
        }
        // More than 1 possibility for the corner
        else {
			// if we have more corners then those may help us ID the corner
			if (numCorners > 1) {
				if 	(i->getShape() == T) {
					if (tCorner > 1) {
						// for now we'll just toss these
						// @TODO: Theoretically we can classify these
						if (debugIdentifyCorners) {
							cout << "Two Ts found - for now we throw them both out"
                                 << endl;
						}
						corners.clear();
						return;
					}
				}
			}

            i->setPossibleCorners(possibleClassifications);
            i->identifyLinesInCorner();
            ++i;
        }
    }

    for (list <VisualCorner>::iterator i = corners.begin();
		 i != corners.end(); ++i){
		i->identifyFromLines();
		i->identifyLinesInCorner();
		if (debugIdentifyCorners) {
			printPossibilities(i->getPossibleCorners());
        }
	}

	// If our corners have no identity, set them to their shape possibilities
    for (list <VisualCorner>::iterator i = corners.begin();
		 i != corners.end(); ++i){
		if (i->getPossibleCorners().empty()) {
			i->setPossibleCorners(ConcreteCorner::getPossibleCorners(
                                      i->getShape(),
                                      i->getSecondaryShape()));
        }
	}
}

/* Loop through the corners looking for corners that may be a sideline.  If
   we see one that may help with context, plus it narrows the possibilities
   for that corner.

   *param corners        the complete list of corners
 */
void Context::checkForSidelineInformation(list<VisualCorner> &corners) {
    for (list <VisualCorner>::iterator i = corners.begin();i != corners.end();
         ++i){
		// before we start, analyze the corner a bit more
		if (i->getSecondaryShape() == UNKNOWN && i->getShape() == T &&
            face == FACING_UNKNOWN) {
			// really long TStems indicate that we have a center T
			// in this particular case sometimes we should be able to absolutely
			// identify the T - if the stem is pointing relatively left or right
            if (realLineDistance(i->getTStem()) >
                GOALBOX_FUDGE * GOALBOX_DEPTH) {
				i->setSecondaryShape(SIDE_T);
                if (face == FACING_UNKNOWN) {
                    if (debugIdentifyCorners) {
                        cout << "Updating facing to sideline" << endl;
                    }
                    face == FACING_SIDELINE;
                }
			}
		}
    }
}

/* Iterate through all of the corners to see if any of them are directly
   connected.  If so, find out what sort of information we can glean from
   that connection (e.g. what sort of T we're looking at).  We can also
   find information know that two corners are unconnected.  So look at that
   as well.

   @param corners      the complete list of corners
 */
void Context::checkForConnectedCorners(list<VisualCorner> &corners) {
    int cornerNumber = 0;
    list <VisualCorner>::iterator i = corners.begin();
    for ( ; i != corners.end(); i++){
        cornerNumber++;
        // find out if the current corner is connected to any other
        list <VisualCorner>::iterator j = corners.begin();
        for (int k = 1; j != corners.end(); j++, k++) {
            if (k > cornerNumber) {
                if (i->getLine1() == j->getLine1() ||
                    i->getLine1() == j->getLine2() ||
                    i->getLine2() == j->getLine1() ||
                    i->getLine2() == j->getLine2()) {
                    findCornerRelationship(*i, *j);
                } else {
                    findUnconnectedCornerRelationship(*i, *j);
                }
            }
        }
    }
}

/** If we have a single T corner we can often glean a lot of information
    about what it might be - especially if it is connected to a field object.
    The length of the stem may tell us that it is a sideline T for example.

    @param  first    A T corner (usually unconnected but not always)
 */
void Context::classifyT(VisualCorner & first) {
    float l1 = realLineDistance(first.getTStem());
    float l2 = realLineDistance(first.getTBar());
    if (l1 > 2 * GOALBOX_DEPTH &&
        (face == FACING_UNKNOWN || !sameHalf || l1 > 3 * GOALBOX_DEPTH)) {
        if (face == FACING_BLUE_GOAL) {
            if (first.doesItPointRight()) {
                first.setSecondaryShape(CENTER_T_BOTTOM);
            } else {
                first.setSecondaryShape(CENTER_T_TOP);
            }
        } else if (face == FACING_YELLOW_GOAL) {
            if (first.doesItPointRight()) {
                first.setSecondaryShape(CENTER_T_TOP);
            } else {
                first.setSecondaryShape(CENTER_T_BOTTOM);
            }
        } else {
            first.setSecondaryShape(SIDE_T);
            face = FACING_SIDELINE;
        }
    } else {
        if (face == FACING_BLUE_GOAL) {
            // This could be made more robust
            if (first.doesItPointUp()) {
                // check if the T is above or below the post
                if (first.getY() > objectRightY) {
                    if (first.doesItPointLeft()) {
                        first.setSecondaryShape(LEFT_GOAL_BLUE_T);
                    } else {
                        first.setSecondaryShape(RIGHT_GOAL_BLUE_T);
                    }
                } else {
                    if (first.doesItPointLeft()) {
                        first.setSecondaryShape(RIGHT_GOAL_BLUE_T);
                    } else {
                        first.setSecondaryShape(LEFT_GOAL_BLUE_T);
                    }
                }
            } else if (first.getX() > objectRightX) {
                first.setSecondaryShape(LEFT_GOAL_BLUE_T);
            } else {
                first.setSecondaryShape(RIGHT_GOAL_BLUE_T);
            }
        } else if (face == FACING_YELLOW_GOAL) {
            if (first.doesItPointUp()) {
                if (first.getY() > objectRightY) {
                    if (first.doesItPointLeft()) {
                        first.setSecondaryShape(LEFT_GOAL_YELLOW_T);
                    } else {
                        first.setSecondaryShape(RIGHT_GOAL_YELLOW_T);
                    }
                } else {
                    if (first.doesItPointLeft()) {
                        first.setSecondaryShape(RIGHT_GOAL_BLUE_T);
                    } else {
                        first.setSecondaryShape(LEFT_GOAL_BLUE_T);
                    }
                }
            } else if (first.getX() > objectRightX) {
                first.setSecondaryShape(LEFT_GOAL_YELLOW_T);
            } else {
                first.setSecondaryShape(RIGHT_GOAL_YELLOW_T);
            }
        } else if (l2 * 2 > FIELD_HEIGHT) {
            first.setSecondaryShape(SIDE_T);
        }
    }
}

/** If we have a single OUTER_L corner we can often glean a lot of information
    about what it might be - especially if it is connected to a field object.
    Our main goal here is to see if we can tell if it is a left or a right
    corner.

    @param first    An unconnected outerl corner
 */
void Context::classifyOuterL(VisualCorner & first) {
    float l1 = realLineDistance(first.getLine1());
    float l2 = realLineDistance(first.getLine2());
    if (debugIdentifyCorners) {
        cout << "Lines " << l1 << " " << l2 << endl;
    }
    // eventually we should be able to figure some stuff out anyway
    // but let's get the low-hanging fruit first
    if (l1 < GOALBOX_FUDGE * GOALBOX_DEPTH &&
        l2 < GOALBOX_FUDGE * GOALBOX_DEPTH) {
        return;
    } else if (l1 > GOALBOX_FUDGE * GOALBOX_DEPTH &&
               l2 > GOALBOX_FUDGE * GOALBOX_DEPTH &&
               !sameHalf) {
        // our "L" is actually a T unfortunately it isn't set up right
        first.setShape(T);
        first.setSecondaryShape(SIDE_T);
        return;
    }
    bool line1IsLonger = l1 > l2;
    bool pointsMostlyUp = abs(first.getOrientation()) < 45.0;
    if (!pointsMostlyUp) {
        // determine which endpoint is higher up on screen
        int line1YMax = first.getLine1()->getTopEndpoint().y;
        if (abs(first.getLine1()->getTopEndpoint().y - first.getY()) <
            abs(first.getLine1()->getBottomEndpoint().y - first.getY())) {
            line1YMax = first.getLine1()->getBottomEndpoint().y;
        }
        int line2YMax = first.getLine2()->getTopEndpoint().y;
        if (abs(first.getLine2()->getTopEndpoint().y - first.getY()) <
            abs(first.getLine2()->getBottomEndpoint().y - first.getY())) {
            line2YMax = first.getLine2()->getBottomEndpoint().y;
        }
        bool line1IsUp = line1YMax < line2YMax;
        if (abs(line1YMax-line2YMax) < 5) {
            line1IsUp = first.getLine1()->getAngle() >
                first.getLine2()->getAngle();
        }
        bool topIsLonger = ((line1IsUp && line1IsLonger) ||
                            (!line1IsUp && !line1IsLonger));
        if (debugIdentifyCorners) {
            cout << "Single Outer L" << endl;
            cout << "Line1: " << l1 << " " << line1YMax << endl;
            cout << "Line2: " << l2 << " " << line2YMax << endl;
            cout << "angles " << first.getLine1()->getAngle() <<
                " " << first.getLine2()->getAngle() << endl;
            if (first.doesItPointRight()) {
                cout << "Corner points to the right" << endl;
            } else {
                cout << "Corner points to the left" << endl;
            }
            if (topIsLonger) {
                cout << "Top is longer " << endl;
            } else {
                cout << "Bottom is longer " << endl;
            }
        }
        bool left;
        if (first.doesItPointRight()) {
            if (topIsLonger) {
                first.setSecondaryShape(LEFT_GOAL_L);
                left = true;
            } else {
                first.setSecondaryShape(RIGHT_GOAL_L);
                left = false;
            }
        } else {
            if (topIsLonger) {
                first.setSecondaryShape(RIGHT_GOAL_L);
                left = false;
            } else {
                first.setSecondaryShape(LEFT_GOAL_L);
                left = true;
            }
        }
        if (face == FACING_BLUE_GOAL) {
            if (left) {
                first.setSecondaryShape(LEFT_GOAL_BLUE_L);
            } else {
                first.setSecondaryShape(RIGHT_GOAL_BLUE_L);
            }
        } else if (face == FACING_YELLOW_GOAL) {
            if (left) {
                first.setSecondaryShape(LEFT_GOAL_YELLOW_L);
            } else {
                first.setSecondaryShape(RIGHT_GOAL_YELLOW_L);
            }
        }
    }
}

/** We have identified an innerl as a field corner.  Set it accordingly
 *  based on other information like whether we see goals or not.
 *  @param corner       The innerl corner we want to set
 */
void Context::setFieldCorner(VisualCorner & corner) {
    if (debugIdentifyCorners) {
        cout << "Inner L classified as Field Corner" << endl;
    }
    corner.setSecondaryShape(CORNER_L);
    switch (face) {
    case FACING_BLUE_GOAL:
        if (objectRightX > -1 && objectRightX < IMAGE_WIDTH) {
            if (corner.getX() < objectRightX) {
                corner.setSecondaryShape(BLUE_GOAL_BOTTOM);
            } else {
                corner.setSecondaryShape(BLUE_GOAL_TOP);
            }
        }
        break;
    case FACING_YELLOW_GOAL:
        if (objectRightX > -1 && objectRightX < IMAGE_WIDTH) {
            if (corner.getX() < objectRightX) {
                corner.setSecondaryShape(YELLOW_GOAL_TOP);
            } else {
                corner.setSecondaryShape(YELLOW_GOAL_BOTTOM);
            }
        }
        break;
    default:
        break;
    }
}

/** This is called when we have an INNER_L and little to suggest
 *  that it is a goal corner (e.g. no long line, no goal box match).
 *  So we look for evidence that it is a field corner.  Among the
 *  the things we look at are the length of its lines - e.g. if
 *  they are both long it is strong evidence - and the distance
 *  to the sideline.
 *  @param first     the innerl we are examining
 *  @param l1        the (real) length of its line1
 *  @param l2        the (real) length of its line2
 */
void Context::lookForFieldCorner(VisualCorner & first, float l1, float l2) {
    // we'll want the distance to the field edge
    int horizon = field->horizonAt(first.getX());
    float dist = realDistance(first.getX(), first.getY(),
                              first.getX(), horizon);
    // we'll want to know the spatial layout of the two lines
    int l1x1 = first.getLine1()->getLeftEndpoint().x;
    int l2x1 = first.getLine2()->getLeftEndpoint().x;
    int l1y1, l2y1;
    // there are two ways the two lines can be connected
    bool l1IsLeft = l1x1 < l2x1;
    // if both lines are long then its a corner
    if (l1 > GOALBOX_DEPTH * GOALBOX_FUDGE &&
        l2 > GOALBOX_DEPTH * GOALBOX_FUDGE) {
        setFieldCorner(first);
    } else if (dist < 2 * 70.0) {
        // if we have a long line and it isn't far from edge -> field corner
        int otherX, otherY;
        if (l1 > GOALBOX_DEPTH * GOALBOX_FUDGE) {
            if (l1IsLeft) {
                otherX = first.getLine1()->getLeftEndpoint().x;
                otherY = first.getLine1()->getLeftEndpoint().y;
            } else {
                otherX = first.getLine1()->getRightEndpoint().x;
                otherY = first.getLine1()->getRightEndpoint().y;
            }
            int horizon2 = field->horizonAt(otherX);
            float dist2 = realDistance(otherX, otherY, otherX, horizon);
            if (dist2 < 2 * 70.0) {
                setFieldCorner(first);
                return;
            }
        } else if (l2 > GOALBOX_DEPTH * GOALBOX_FUDGE) {
            if (!l1IsLeft) {
                otherX = first.getLine2()->getLeftEndpoint().x;
                otherY = first.getLine2()->getLeftEndpoint().y;
            } else {
                otherX = first.getLine2()->getRightEndpoint().x;
                otherY = first.getLine2()->getRightEndpoint().y;
            }
            int horizon2 = field->horizonAt(otherX);
            float dist2 = realDistance(otherX, otherY, otherX, horizon);
            if (dist2 < 2 * 70.0) {
                setFieldCorner(first);
                return;
            }
        } else {
            // alternatively if all three points are close to edge
            int x1 = first.getLine1()->getLeftEndpoint().x;
            int y1 = first.getLine1()->getLeftEndpoint().y;
            int x2 = first.getLine2()->getRightEndpoint().x;
            int y2 = first.getLine2()->getRightEndpoint().y;
            if (!l1IsLeft) {
                first.getLine1()->getRightEndpoint().x;
                first.getLine1()->getRightEndpoint().y;
                first.getLine2()->getLeftEndpoint().x;
                first.getLine2()->getLeftEndpoint().y;
            }
            int h1 = field->horizonAt(x1);
            int h2 = field->horizonAt(x2);
            float d1 = realDistance(x1, y1, x1, h1);
            float d2 = realDistance(x2, y2, x2, h2);
            if (d1 < 70.0 * 2 && d2 < 70.0 * 2) {
                setFieldCorner(first);
                return;
            }
        }
    }
    if (debugIdentifyCorners) {
        cout << "Lone InnerL, Distance to horizon is " << dist << endl;
        cout << "      " << horizon << " " << first.getY() << endl;
        cout << "Lengths: " << l1 << " " << l2 << endl;
    }
}


/** If we have a single INNER_L corner we can often glean a lot of information
    about what it might be - especially if it is connected to a field object.
    Things we can check:
          Can we see the goal box?
          Can we see a goal post?
          How long are the lines in the corner?
    @param first     the innerl (not connected to another corner)
 */
void Context::classifyInnerL(VisualCorner & first) {
    // we'll need the actual lengths of each line
    float l1 = realLineDistance(first.getLine1());
    float l2 = realLineDistance(first.getLine2());

    // punt (for now) when we can be sure about what we see
    if (!seeGoalBoxLines && face == FACING_UNKNOWN) {
        lookForFieldCorner(first, l1, l2);
        return;
    }
    // we'll want the distance to the field edge
    int horizon = field->horizonAt(first.getX());
    float dist = realDistance(first.getX(), first.getY(),
                              first.getX(), horizon);
    // we'll want to know the spatial layout of the two lines
    int l1x1 = first.getLine1()->getLeftEndpoint().x;
    int l2x1 = first.getLine2()->getLeftEndpoint().x;
    int l1y1, l2y1;
    // there are two ways the two lines can be connected
    bool l1IsLeft = l1x1 < l2x1;

    // Watch out for seeing a corner, but a part of goalbox too, but
    // the T of the goalbox is too close to edge to recognize
    // replace 70 with GREEN_PAD_X
    if (dist < 70.0 * 2.0 && first.getDistance() > 175) {
        // could indicate this is actually a corner
        // we can do this if we see a post
        // punt for now
        return;
    }
    // l1 and l2 hold information on points away from the corner
    if (l1IsLeft) {
        l2x1 = first.getLine2()->getRightEndpoint().x;
        l2y1 = first.getLine2()->getRightEndpoint().y;
        l1y1 = first.getLine1()->getLeftEndpoint().y;
    } else {
        l1x1 = first.getLine1()->getRightEndpoint().x;
        l1y1 = first.getLine1()->getRightEndpoint().y;
        l2y1 = first.getLine2()->getLeftEndpoint().y;
    }
    // determine which endpoint is higher up on screen
    bool l1IsHigher = l1y1 < l2y1;
    bool l1IsLonger = l1 > l2;
    // determine general facing
    if (face == FACING_UNKNOWN) {
        // we have one corner and no field objects
        first.setSecondaryShape(GOAL_L);
        // now see if we can figure out exactly which L
        if (l1IsLonger) {
            if (l1 > GOALBOX_FUDGE * GOALBOX_DEPTH) {
                // we have enough information - is big line to left or right?
                if (l1IsLeft) {
                    first.setSecondaryShape(RIGHT_GOAL_L);
                } else {
                    first.setSecondaryShape(LEFT_GOAL_L);
                }
            }
        } else if (l2 > GOALBOX_FUDGE * GOALBOX_DEPTH) {
            if (!l1IsLeft) {
                first.setSecondaryShape(RIGHT_GOAL_L);
            } else {
                first.setSecondaryShape(LEFT_GOAL_L);
            }
        } else {
            // The situation: one of the lines is parallel to the sideline
            // if we can figure out which one, then we know it is the
            // shorter line
            if (first.getLine1()->isParallel() &&
                !first.getLine2()->isParallel()) {
                // line 1 is parallel to the sideline, so we can infer corner
                if (l1IsLeft) {
                    first.setSecondaryShape(LEFT_GOAL_L);
                } else {
                    first.setSecondaryShape(RIGHT_GOAL_L);
                }
            } else if (!first.getLine1()->isParallel() &&
                       first.getLine2()->isParallel()) {
                if (!l1IsLeft) {
                    first.setSecondaryShape(LEFT_GOAL_L);
                } else {
                    first.setSecondaryShape(RIGHT_GOAL_L);
                }
            }
        }
    } else {
        shape leftColor = LEFT_GOAL_YELLOW_L;
        shape rightColor = RIGHT_GOAL_YELLOW_L;
        if ((face == FACING_BLUE_GOAL && sameHalf) ||
            (face == FACING_YELLOW_GOAL && !sameHalf)) {
            leftColor = LEFT_GOAL_BLUE_L;
            rightColor = RIGHT_GOAL_BLUE_L;
        }
        // we see an inner l and a goal post
        // now see if we can figure out exactly which L
        // sometime to be super-safe we should check where the line intersects
        // the goal post
        // best done by using the post
        if (objectRightX > -1 && objectRightX < IMAGE_WIDTH / 2) {
            // post is on the left, normally this indicates a left goal inner L
            // the line should basically disappear into the middle of the post
            if (l1IsLeft) {
                if (l1y1 < objectRightY) {
                    first.setSecondaryShape(leftColor);
                } else {
                    // we must have missed the other L corner for some reason
                    first.setSecondaryShape(RIGHT_GOAL_CORNER);
                }
            } else {
                if (l2y1 < objectRightY) {
                    first.setSecondaryShape(leftColor);
                } else {
                    first.setSecondaryShape(RIGHT_GOAL_CORNER);
                }
            }
        } else {
            // the post is on the right - normally a right corner
            if (l1IsLeft) {
                if (l2y1 < objectRightY) {
                    first.setSecondaryShape(rightColor);
                } else {
                    first.setSecondaryShape(LEFT_GOAL_CORNER);
                }
            } else {
                if (l1y1 < objectRightY) {
                    first.setSecondaryShape(rightColor);
                } else {
                    first.setSecondaryShape(LEFT_GOAL_CORNER);
                }
            }
        }
    }
}

/** We see two inner Ls.  This is quite easy to handle, figure out which
    one is inside, then look at whether it is right or left of the other
    one.
    @param inner      the corner inside of the other
    @param outer      the corner outside of the other
 */
void Context::unconnectedInnerLs(VisualCorner & inner, VisualCorner & outer) {
    // inner is a field corner, outer a goal corner
    // determine which ones
    if (inner.getX() < outer.getX()) {
        if (face == FACING_BLUE_GOAL) {
            inner.setSecondaryShape(RIGHT_GOAL_BLUE_L);
            outer.setSecondaryShape(BLUE_GOAL_BOTTOM);
        } else if (face == FACING_YELLOW_GOAL) {
            inner.setSecondaryShape(RIGHT_GOAL_YELLOW_L);
            outer.setSecondaryShape(YELLOW_GOAL_TOP);
        } else {
            inner.setSecondaryShape(RIGHT_GOAL_L);
            outer.setSecondaryShape(RIGHT_GOAL_CORNER);
        }
    } else {
        if (face == FACING_BLUE_GOAL) {
            inner.setSecondaryShape(LEFT_GOAL_BLUE_L);
            outer.setSecondaryShape(BLUE_GOAL_TOP);
        } else if (face == FACING_YELLOW_GOAL) {
            inner.setSecondaryShape(LEFT_GOAL_YELLOW_L);
            outer.setSecondaryShape(YELLOW_GOAL_BOTTOM);
        } else {
            inner.setSecondaryShape(LEFT_GOAL_L);
            outer.setSecondaryShape(LEFT_GOAL_CORNER);
        }
    }
}

/** Given two corners that we know are not connected, explore to find out the
    possible relationships.  This should help us reduce the number of
    possible corners each can be and ultimately help the identification
    process.  Normally this is two innerls (goal corner, field corner).
    @param first       one corner
    @param second      the other
 */
void Context::findUnconnectedCornerRelationship(VisualCorner & first,
                                                VisualCorner & second) {
    VisualCorner* inner = &first;
    VisualCorner* outer = &second;
    cout << "Testing non connected" << endl;
    if (first.getShape() == INNER_L) {
        if (second.getShape() == OUTER_L) {
            inner = &first;
            outer = &second;
        } else if (second.getShape() == INNER_L) {
            // Should be an easy case - find the "inner" innerl
            if (first.getY() < second.getY()) {
                unconnectedInnerLs(second, first);
            } else {
                unconnectedInnerLs(first, second);
            }
            return;
        }
    } else if (second.getShape() == INNER_L) {
        if (first.getShape() == OUTER_L) {
            inner = &second;
            outer = &first;
        } else {
            return;
        }
    } else {
        return;
    }
    // check the orientation of the outer-l
    if (outer->doesItPointRight()) {
        inner->setSecondaryShape(RIGHT_GOAL_CORNER);
        if (face == FACING_BLUE_GOAL) {
            outer->setSecondaryShape(RIGHT_GOAL_BLUE_L);
        } else if (face == FACING_YELLOW_GOAL) {
            outer->setSecondaryShape(RIGHT_GOAL_YELLOW_L);
        } else {
            outer->setSecondaryShape(RIGHT_GOAL_L);
        }
        return;
    } else {
        inner->setSecondaryShape(LEFT_GOAL_CORNER);
        if (face == FACING_BLUE_GOAL) {
            outer->setSecondaryShape(LEFT_GOAL_BLUE_L);
        } else if (face == FACING_YELLOW_GOAL) {
            outer->setSecondaryShape(LEFT_GOAL_YELLOW_L);
        } else {
            outer->setSecondaryShape(LEFT_GOAL_L);
        }
        return;
    }
}

/** We have connected T corners.  As this is impossible
    we should explore it further and see if one is a center circle.
    When this is called the common line should be first.getTStem()
    @param first        a T corner
    @param second       another T corner
*/
void Context::checkConnectedTs(VisualCorner & first, VisualCorner & second) {
    if (realLineDistance(first.getTStem()) > GOALBOX_DEPTH * GOALBOX_FUDGE) {
        classifyT(first);
        if (first.getSecondaryShape() == CENTER_T_TOP) {
            second.setSecondaryShape(CENTER_CIRCLE_TOP);
        } else if (first.getSecondaryShape() == CENTER_T_BOTTOM) {
            second.setSecondaryShape(CENTER_CIRCLE_BOTTOM);
        } else {
            second.setShape(CIRCLE);
        }
        tCorner--;
    }
}

/** We have a T connected to a center circle.  This is a nice easy case.
    See what the T is and potentially use that to narrow the circle.
    @param t       A T corner
    @param center  A center circle corner
 */
void Context::checkTToCenter(VisualCorner & t, VisualCorner & center) {
    classifyT(t);
    if (t.getSecondaryShape() == CENTER_T_TOP) {
        center.setSecondaryShape(CENTER_CIRCLE_TOP);
    } else if (t.getSecondaryShape() == CENTER_T_BOTTOM) {
        center.setSecondaryShape(CENTER_CIRCLE_BOTTOM);
    }
}

/** We have a T connected to a goalbox corner.  Theoretically the
    T could be anything, so try and narrow it down.
    @param   t    A T corner
    @param l1     A corner connected to the T
    @param common The line that the two corners have in common
 */
void Context::checkTToGoal(VisualCorner & t, VisualCorner & l1,
                           boost::shared_ptr<VisualLine> common) {
    t.setSecondaryShape(GOAL_T);
    l1.setSecondaryShape(GOAL_L);
    // looks very good - ultimately we should check line length too
    if (debugIdentifyCorners) {
        cout << "T connect to an L, should be goal box " <<
            realLineDistance(t.getTStem()) << " " << l1.getX() << endl;
    }
    // can we determine which side?
    if (l1.getShape() == OUTER_L) {
        // look at the non-common line, figure out which direction it goes
        // if it goes in the same as left T, then it is a RIGHT L
        point<int> left;
        point<int> right;
        point<int> tpoint = t.getTStemEndpoint();
        if (l1.getLine1() == common) {
            left = l1.getLine2()->getLeftEndpoint();
            right = l1.getLine2()->getRightEndpoint();
        } else {
            left = l1.getLine1()->getLeftEndpoint();
            right = l1.getLine1()->getRightEndpoint();
        }
        // unfortunately the t-endpoint doesn't always equal the corner
        int close = abs(left.x - tpoint.x) + abs(left.y - tpoint.y);
        int close2 = abs(right.x - tpoint.x) + abs(right.y - tpoint.y);
        if (close > close2) {
            // Line goes to right of T
            if (face == FACING_BLUE_GOAL) {
                l1.setSecondaryShape(LEFT_GOAL_BLUE_L);
                t.setSecondaryShape(LEFT_GOAL_BLUE_T);
            } else if (face == FACING_YELLOW_GOAL) {
                l1.setSecondaryShape(LEFT_GOAL_YELLOW_L);
                t.setSecondaryShape(LEFT_GOAL_YELLOW_T);
            } else {
                l1.setSecondaryShape(LEFT_GOAL_L);
                t.setSecondaryShape(LEFT_GOAL_T);
            }
        } else {
            if (face == FACING_BLUE_GOAL) {
                l1.setSecondaryShape(RIGHT_GOAL_BLUE_L);
                t.setSecondaryShape(RIGHT_GOAL_BLUE_T);
            } else if (face == FACING_YELLOW_GOAL) {
                l1.setSecondaryShape(RIGHT_GOAL_YELLOW_L);
                t.setSecondaryShape(RIGHT_GOAL_YELLOW_T);
            } else {
                l1.setSecondaryShape(RIGHT_GOAL_L);
                t.setSecondaryShape(RIGHT_GOAL_T);
            }
        }
    }
}

/** We have a T and (apparently) a field corner. The T could be
    a side or a goal, so try and use length, # of corners and
    other things to narrow it down.
    @param t    The T corner
    @param l1   A corner connected to the T (innerl)
 */
void Context::checkTToFieldCorner(VisualCorner & t, VisualCorner & l1) {
    if (debugIdentifyCorners) {
        cout << "T connect to an L should be goal line to corner " <<
            realLineDistance(t.getTBar()) << endl;
    }
    l1.setSecondaryShape(CORNER_L);
    if (lCorner > 1) {
        // we should be able to narrow the choices down to 2
        // compare the location of the L corner to the T corner
        if (l1.getX() < t.getX()) {
            if (face == FACING_BLUE_GOAL) {
                t.setSecondaryShape(RIGHT_GOAL_BLUE_T);
                l1.setSecondaryShape(BLUE_GOAL_BOTTOM);
            } else if (face == FACING_YELLOW_GOAL) {
                t.setSecondaryShape(RIGHT_GOAL_YELLOW_T);
                l1.setSecondaryShape(YELLOW_GOAL_TOP);
            } else {
                t.setSecondaryShape(RIGHT_GOAL_T);
                l1.setSecondaryShape(RIGHT_GOAL_CORNER);
            }
        } else {
            if (face == FACING_BLUE_GOAL) {
                t.setSecondaryShape(LEFT_GOAL_BLUE_T);
                l1.setSecondaryShape(BLUE_GOAL_TOP);
            } else if (face == FACING_YELLOW_GOAL) {
                t.setSecondaryShape(LEFT_GOAL_YELLOW_T);
                l1.setSecondaryShape(YELLOW_GOAL_BOTTOM);
            } else {
                l1.setSecondaryShape(LEFT_GOAL_CORNER);
                t.setSecondaryShape(LEFT_GOAL_T);
            }
        }
    } else {
        // check length  -- it its REALLY long to corner then we know
        // its a sideline T
        float dist = getEstimatedDistance(l1.getDistance(), l1.getBearing(),
                                          t.getDistance(), t.getBearing());
        // figure out if lcorner is left or right of T
        bool left = false;
        if (t.getLocation().x < l1.getLocation().x) {
            left = true;
        }
        if (dist * 2 < FIELD_WHITE_WIDTH) {
            // seems to be a goalline - evaluate further
            if (left) {
                if (face == FACING_BLUE_GOAL) {
                    t.setSecondaryShape(LEFT_GOAL_BLUE_T);
                    l1.setSecondaryShape(BLUE_GOAL_TOP);
                } else if (face == FACING_YELLOW_GOAL) {
                    t.setSecondaryShape(LEFT_GOAL_YELLOW_T);
                    l1.setSecondaryShape(YELLOW_GOAL_BOTTOM);
                } else {
                    t.setSecondaryShape(LEFT_GOAL_T);
                    l1.setSecondaryShape(LEFT_GOAL_CORNER);
                }
            } else {
                if (face == FACING_BLUE_GOAL) {
                    t.setSecondaryShape(RIGHT_GOAL_BLUE_T);
                    l1.setSecondaryShape(BLUE_GOAL_BOTTOM);
                } else if (face == FACING_YELLOW_GOAL) {
                    t.setSecondaryShape(RIGHT_GOAL_YELLOW_T);
                    l1.setSecondaryShape(YELLOW_GOAL_TOP);
                } else {
                    t.setSecondaryShape(RIGHT_GOAL_T);
                    l1.setSecondaryShape(RIGHT_GOAL_CORNER);
                }
            }
        } else {
            face = FACING_SIDELINE;
            t.setSecondaryShape(SIDE_T);
            if (left) {
                l1.setSecondaryShape(RIGHT_GOAL_CORNER);
            } else {
                l1.setSecondaryShape(LEFT_GOAL_CORNER);
            }
        }
    }
}

/** Given two corners that we know are connected, explore to find out the
    possible relationships.  This should help us reduce the number of
    possible corners each can be and ultimately help the identification
    process.  Since every corner on the field is connected to Ts (except
    the top two goal box corners) that is what we focus on.
    @param first    a connected corner
    @param second   the corner the first corner was connected to
 */
void Context::findCornerRelationship(VisualCorner & first,
                                     VisualCorner & second) {
    boost::shared_ptr<VisualLine> common;
    if (first.getLine1() == second.getLine1()) {
        common = first.getLine1();
    } else if (first.getLine1() == second.getLine2()) {
        common = first.getLine1();
    } else {
        common = first.getLine2();
    }
    if (first.getShape() == T) {
        if (second.getShape() == OUTER_L ||
            second.getShape() == INNER_L) {
            if (first.getTStem() == common) {
                checkTToGoal(first, second, common);
            } else {
                checkTToFieldCorner(first, second);
            }
        } else {
            // T to CIRCLE or T to T
            if (second.getShape() == T) {
                if (common == first.getTStem()) {
                    checkConnectedTs(first, second);
                } else {
                    checkConnectedTs(second, first);
                }
            } else if (second.getShape() == CIRCLE) {
                checkTToCenter(first, second);
            }
        }
    } else if (second.getShape() == T) {
        if (first.getShape() == OUTER_L ||
            first.getShape() == INNER_L) {
            if (second.getTStem() == common) {
                checkTToGoal(second, first, common);
            } else {
                checkTToFieldCorner(second, first);
            }
        } else {
            checkTToCenter(second, first);
        }
    } else {
        if (debugIdentifyCorners) {
            cout << "Two non T corners with common length " <<
                realLineDistance(common) << endl;
        }
        // it is likely that one of the corners is actually a T
        // it is possible (never seen it) that it could be the two goal corners
    }
}

/**
 * Compare the given VisualCorner with other visible objects
 *  to see if the distances fit any sort of ConcreteCorners.
 *  Sets the possiblilities of the VisualCorner.
 *
 *  @param corner Corner to compare with VisualFieldObjects
 *  @param visibleObjects Visible objects to compare corner dists to.
 *  @return List of all ConcreteCorners whose distances fit the given objects.
 */
const list<const ConcreteCorner*> Context::classifyCornerWithObjects(
    const VisualCorner &corner,
    const vector <const VisualFieldObject*> &visibleObjects) const
{

	// Get all the possible corners given the shape of the corner
	vector <const ConcreteCorner*> possibleCorners =
		ConcreteCorner::getPossibleCorners(corner.getShape(),
                                           corner.getSecondaryShape());

    if (debugIdentifyCorners) {
        cout << endl
             << "Beginning to get possible classifications for corner given "
             << visibleObjects.size() << " visible objects and "
             << possibleCorners.size() << " corner possibilities" << endl
             << endl;
    }

    // sometimes we have already classified the corner, so just return it
    if (possibleCorners.size() == 1) {
        list<const ConcreteCorner*> classification;
        vector<const ConcreteCorner*>::const_iterator j =
            possibleCorners.begin();
        classification.push_back(*j);
        return classification;
    }

	// Get all the possible corners given the shape of the corner
	list<const ConcreteCorner*> possibleClassifications;
    switch(corner.getShape()) {
    case T:
        possibleClassifications =
            compareObjsT(corner, possibleCorners, visibleObjects);
        break;
    case OUTER_L:
        possibleClassifications =
            compareObjsOuterL(corner, possibleCorners, visibleObjects);
        break;
    case INNER_L:
        possibleClassifications =
            compareObjsInnerL(corner, possibleCorners, visibleObjects);
        break;
    default:
        possibleClassifications =
            compareObjsCorners(corner, possibleCorners, visibleObjects);
        break;
    }

	// If we found nothing that time, try again with all the corners to
	// see if possibly the corner was misidentified (e.g. saw an L, but
	// actually a T)
	if (possibleClassifications.empty()){

		//possibleCorners = ConcreteCorner::concreteCorners();
		//possibleClassifications =
		//	compareObjsCorners(corner, possibleCorners, visibleObjects);
        if (possibleClassifications.empty()) {
            // reset to the best we've got
            possibleCorners =
                ConcreteCorner::getPossibleCorners(corner.getShape(),
                                                   corner.getSecondaryShape());
            list<const ConcreteCorner*> classification;
            vector<const ConcreteCorner*>::const_iterator j =
                possibleCorners.begin();
            for (; j != possibleCorners.end(); ++j) {
                classification.push_back(*j);
            }
            return classification;
        }
	}

    return possibleClassifications;
}

/* Given a list of concrete corners that the Outer_L could possibly be,
   weeds out the bad ones based on distances to visible objects and returns
   those that are still in the running.
   @param corner            the corner we are testing
   @param possibleCorners   a list of real corners it might be
   @param visibleObjects    a list of goal posts that we see
*/
list <const ConcreteCorner*> Context::compareObjsOuterL(
	const VisualCorner& corner,
	const vector<const ConcreteCorner*>& possibleCorners,
	const vector<const VisualFieldObject*>& visibleObjects) const
{
	list<const ConcreteCorner*> possibleClassifications;

    // For each field object that we see, calculate its real distance to
    // each possible concrete corner and compare with the visual estimated
    // distance. If it fits, add it to the list of possibilities.
    vector<const ConcreteCorner*>::const_iterator j =
        possibleCorners.begin();

    for (; j != possibleCorners.end(); ++j) {
		bool isOk = true;
        for (vector <const VisualFieldObject*>::const_iterator k =
                 visibleObjects.begin();
             k != visibleObjects.end() && isOk; ++k) {

            if (face == FACING_BLUE_GOAL) {
                if ((*j)->getID() == YELLOW_GOAL_LEFT_L ||
                    (*j)->getID() == YELLOW_GOAL_RIGHT_L) {
                    isOk = false;
                    continue;
                }
            } else if (face == FACING_YELLOW_GOAL) {
                if ((*j)->getID() == BLUE_GOAL_LEFT_L ||
                    (*j)->getID() == BLUE_GOAL_RIGHT_L) {
                    isOk = false;
                    continue;
                }
            }
            const float estimatedDistance = getEstimatedDistance(&corner, *k);
            const float distanceToCorner = corner.getDistance();
			// The visual object might be abstract, so we should check
			// all of its possible objects to see if we're close enough to one
			// and add all the possibilities up.
			list<const ConcreteFieldObject*>::const_iterator i =
				(*k)->getPossibleFieldObjects()->begin();

			bool close = false;
			for (int p = 0; i != (*k)->getPossibleFieldObjects()->end() &&
                     !close; ++i, ++p) {

				if (arePointsCloseEnough(estimatedDistance, *j, *k,
										 distanceToCorner, p)) {
					close = true;
                }
            }
			// if the corner wasn't close enough to any possible object
			if (!close) {
				isOk = false;
			}
		}
		// if we made it through all possible field objects
		if (isOk) {
			possibleClassifications.push_back(*j);
			if (debugIdentifyCorners) {
				cout << "Corner is possibly a " << (*j)->toString() << endl;
			}
		}
	}
	return possibleClassifications;
}

/* Given a list of concrete corners that the T corner could possibly be,
   weeds out the bad ones based on distances to visible objects and returns
   those that are still in the running.
   @param corner            the corner we are testing
   @param possibleCorners   a list of real corners it might be
   @param visibleObjects    a list of goal posts that we see
*/
list <const ConcreteCorner*> Context::compareObjsT(
	const VisualCorner& corner,
	const vector<const ConcreteCorner*>& possibleCorners,
	const vector<const VisualFieldObject*>& visibleObjects) const
{
	list<const ConcreteCorner*> possibleClassifications;

    // For each field object that we see, calculate its real distance to
    // each possible concrete corner and compare with the visual estimated
    // distance. If it fits, add it to the list of possibilities.
    vector<const ConcreteCorner*>::const_iterator j =
        possibleCorners.begin();

    // Note: changed the order of the loops 12/3/2010 so we can check
    // every object against the corner instead of just finding one good one
    for (; j != possibleCorners.end(); ++j) {
		bool isOk = true;
        for (vector <const VisualFieldObject*>::const_iterator k =
                 visibleObjects.begin(); k != visibleObjects.end() && isOk;
             ++k) {

            const float estimatedDistance = getEstimatedDistance(&corner, *k);
            /* if we have a T corner and a goal post, then we can determine
               which one it is definitely - look at whether the Stem is going up
               or down if it is down (normal case) just look at whether T is
               left or right if it is up, then reverse the results because you
               are over the endline. */
            if ((*j)->getID() == CENTER_TOP_T ||
                (*j)->getID() == CENTER_BOTTOM_T) {
                // needs to be related to some field constant
                if (estimatedDistance > 250.0) {
                    // we can tell which one by looking at the direction
                    if (corner.getOrientation() > 0.0) {
                        if (face == FACING_BLUE_GOAL) {
                            if ((*j)->getID() == CENTER_BOTTOM_T) {
                                continue;
                            } else {
                                isOk = false;
                                continue;
                            }
                        } else {
                            if ((*j)->getID() == CENTER_TOP_T) {
                                continue;
                            } else {
                                isOk = false;
                                continue;
                            }
                        }
                    } else {
                        if (face == FACING_BLUE_GOAL) {
                            if ((*j)->getID() == CENTER_TOP_T) {
                                continue;
                            } else {
                                isOk = false;
                                continue;
                            }
                        } else {
                            if ((*j)->getID() == CENTER_BOTTOM_T) {
                                continue;
                            } else {
                                isOk = false;
                                continue;
                            }
                        }
                    }
                }
            }
            bool down = abs(corner.getOrientation()) > 90.0;
            bool right;
            if (corner.getX() > (*k)->getX()) {
                right = true;
                cout << "T is right " << estimatedDistance << endl;
            } else {
                right = false;
                cout << "T is left " << estimatedDistance << endl;
            }
            if (down) {
                if (right) {
                    if ((*j)->getID() == BLUE_GOAL_RIGHT_T ||
                        (*j)->getID() == YELLOW_GOAL_RIGHT_T) {
                        isOk = false;
                        continue;
                    }
                } else {
                    if ((*j)->getID() == BLUE_GOAL_LEFT_T ||
                        (*j)->getID() == YELLOW_GOAL_LEFT_T) {
                        isOk = false;
                        continue;
                    }
                }
            } else {
                if (!right) {
                    if ((*j)->getID() == BLUE_GOAL_RIGHT_T ||
                        (*j)->getID() == YELLOW_GOAL_RIGHT_T) {
                        isOk = false;
                        continue;
                    }
                } else {
                    if ((*j)->getID() == BLUE_GOAL_LEFT_T ||
                        (*j)->getID() == YELLOW_GOAL_LEFT_T) {
                        isOk = false;
                        continue;
                    }
                }
            }
            // need a constant related to field constants here
            if (estimatedDistance < 100.0f &&
                getRealDistance(*j, *k, 0) < 100.0f) {
                continue;
            }
            const float distanceToCorner = corner.getDistance();
			// The visual object might be abstract, so we should check
			// all of its possible objects to see if we're close enough to one
			// and add all the possibilities up.
			list<const ConcreteFieldObject*>::const_iterator i =
				(*k)->getPossibleFieldObjects()->begin();

			bool close = false;
			for (int p = 0; i != (*k)->getPossibleFieldObjects()->end() && !close;
                 ++i, ++p) {

				if (arePointsCloseEnough(estimatedDistance, *j, *k,
										 distanceToCorner, p)) {
					close = true;
                }
            }
			// if the corner wasn't close enough to any possible object
			if (!close) {
				isOk = false;
			}
		}
		// if we made it through all possible field objects
		if (isOk) {
			possibleClassifications.push_back(*j);
			if (debugIdentifyCorners) {
				cout << "Corner is possibly a " << (*j)->toString() << endl;
			}
		}
	}
	return possibleClassifications;
}

/* Given a list of concrete corners that the visual corner could possibly be,
   weeds out the bad ones based on distances to visible objects and returns
   those that are still in the running.
   @param corner            the corner we are testing
   @param possibleCorners   a list of real corners it might be
   @param visibleObjects    a list of goal posts that we see
*/

list <const ConcreteCorner*> Context::compareObjsInnerL(
	const VisualCorner& corner,
	const vector<const ConcreteCorner*>& possibleCorners,
	const vector<const VisualFieldObject*>& visibleObjects) const
{
	list<const ConcreteCorner*> possibleClassifications;

    // For each field object that we see, calculate its real distance to
    // each possible concrete corner and compare with the visual estimated
    // distance. If it fits, add it to the list of possibilities.
    vector<const ConcreteCorner*>::const_iterator j =
        possibleCorners.begin();

    // Note: changed the order of the loops 12/3/2010 so we can check
    // every object against the corner instead of just finding one good one
    for (; j != possibleCorners.end(); ++j) {
		bool isOk = true;
        for (vector <const VisualFieldObject*>::const_iterator k =
                 visibleObjects.begin(); k != visibleObjects.end() && isOk;
             ++k) {

            const float estimatedDistance = getEstimatedDistance(&corner, *k);
            const float distanceToCorner = corner.getDistance();
			// The visual object might be abstract, so we should check
			// all of its possible objects to see if we're close enough to one
			// and add all the possibilities up.
			list<const ConcreteFieldObject*>::const_iterator i =
				(*k)->getPossibleFieldObjects()->begin();

			bool close = false;
			for (int p = 0; i != (*k)->getPossibleFieldObjects()->end() && !close;
                 ++i, ++p) {

				if (arePointsCloseEnough(estimatedDistance, *j, *k,
										 distanceToCorner, p)) {
					close = true;
                }
            }
			// if the corner wasn't close enough to any possible object
			if (!close) {
				isOk = false;
			}
		}
		// if we made it through all possible field objects
		if (isOk) {
			possibleClassifications.push_back(*j);
			if (debugIdentifyCorners) {
				cout << "Corner is possibly a " << (*j)->toString() << endl;
			}
		}
	}
	return possibleClassifications;
}

/* Given a list of concrete corners that the visual corner could possibly be,
   weeds out the bad ones based on distances to visible objects and returns
   those that are still in the running.
   @param corner            the corner we are testing
   @param possibleCorners   a list of real corners it might be
   @param visibleObjects    a list of goal posts that we see
*/
list <const ConcreteCorner*> Context::compareObjsCorners(
	const VisualCorner& corner,
	const vector<const ConcreteCorner*>& possibleCorners,
	const vector<const VisualFieldObject*>& visibleObjects) const
{
	list<const ConcreteCorner*> possibleClassifications;

    // For each field object that we see, calculate its real distance to
    // each possible concrete corner and compare with the visual estimated
    // distance. If it fits, add it to the list of possibilities.
    vector<const ConcreteCorner*>::const_iterator j =
        possibleCorners.begin();

    // Note: changed the order of the loops 12/3/2010 so we can check
    // every object against the corner instead of just finding one good one
    for (; j != possibleCorners.end(); ++j) {
		bool isOk = true;
        for (vector <const VisualFieldObject*>::const_iterator k =
                 visibleObjects.begin(); k != visibleObjects.end() && isOk;
             ++k) {

            const float estimatedDistance = getEstimatedDistance(&corner, *k);
            const float distanceToCorner = corner.getDistance();
			// The visual object might be abstract, so we should check
			// all of its possible objects to see if we're close enough to one
			// and add all the possibilities up.
			list<const ConcreteFieldObject*>::const_iterator i =
				(*k)->getPossibleFieldObjects()->begin();

			bool close = false;
			for (int p = 0; i != (*k)->getPossibleFieldObjects()->end() && !close;
                 ++i, ++p) {

				if (arePointsCloseEnough(estimatedDistance, *j, *k,
										 distanceToCorner, p)) {
					close = true;
                }
            }
			// if the corner wasn't close enough to any possible object
			if (!close) {
				isOk = false;
			}
		}
		// if we made it through all possible field objects
		if (isOk) {
			possibleClassifications.push_back(*j);
			if (debugIdentifyCorners) {
				cout << "Corner is possibly a " << (*j)->toString() << endl;
			}
		}
	}
	return possibleClassifications;
}

/**
 * Compares the given estimated distance against the actual distance between
 * the given concrete corner and the field object's true field position. Returns
 * true if the estimated distance is acceptable.
 *
 * @param estimatedDistance Distance between VisualCorner and VisualFieldObject
 * @param j Corner whose distance from the FieldObject is being measured
 * @param k FieldObject used for measuremnts
 * @param distToCorner Distance from robot to corner.
 */
const bool Context::arePointsCloseEnough(const float estimatedDistance,
											const ConcreteCorner* j,
											const VisualFieldObject* k,
											const float distToCorner, int n) const
{
	const float realDistance = getRealDistance(j, k, n);
	const float absoluteError = fabs(realDistance - estimatedDistance);

	const float relativeErrorReal = absoluteError / realDistance * 100.0f;

	// If we have already one good distance between this corner and a
	// field object, we only require that the next objects have a small
	// relative error.
	const float MAX_RELATIVE_ERROR = 70.0f;
	const float USE_RELATIVE_DISTANCE = 250.0f;
    const float MAX_ABSOLUTE_ERROR = 200.f;

    if ( relativeErrorReal < MAX_RELATIVE_ERROR &&
         k->getDistance() > USE_RELATIVE_DISTANCE &&
         distToCorner > USE_RELATIVE_DISTANCE &&
         absoluteError < MAX_ABSOLUTE_ERROR) {
		if (debugIdentifyCorners) {
			cout << "\tDistance between " << j->toString() << " and "
				 << k->toString() << " was fine! Relative error of "
                 << relativeErrorReal
				 << " and absolute error of "
				 << absoluteError
				 << " corner pos: (" << j->getFieldX() << ","
				 << j->getFieldY()
				 << " goal pos: (" << k->getFieldX() << ","
				 << k->getFieldY() << endl;
		}
		return true;
    } else if (absoluteError > getAllowedDistanceError(k) ||
        absoluteError > MAX_ABSOLUTE_ERROR) {
		if (debugIdentifyCorners) {
			cout << "\tDistance between " << j->toString() << " and "
				 << k->toString() << " too large." << endl
				 << "\tReal: " << realDistance
				 << "\tEstimated: " << estimatedDistance << endl
				 << "\tAbsolute error: " << absoluteError
				 << "\tRelative error: " << relativeErrorReal << "% , "
				 << relativeErrorReal << "%"
				 << endl;
		}
		return false;
    } else {
		if (debugIdentifyCorners) {
			cout << "\tDistance between " << j->toString() << " and "
				 << k->toString() << " was fine! Absolute error of "
				 << absoluteError
				 << " corner pos: (" << j->getFieldX() << ","
				 << j->getFieldY() << ")"
				 << " goal pos: (" << k->getFieldX() << ","
				 << k->getFieldY() << ")" << endl;
		}
		return true;
	}
}

// @TODO A real distance error calculation. For now, just uses 2 times
// the FieldObject's Distance SD
/* An artifact of the old field lines code.  May not even be used anymore
   except for corners we're really clueless about.
   @param obj      Object we want to test distance against
   @return         how much error will we tolerate
 */
float Context::getAllowedDistanceError(const VisualFieldObject * obj) const
{
    return obj->getDistanceSD() * 2;
}


/* Uses vector subtraction in order to determine distance between corner and
   field object.
   If the field object is very close (<100 cm away) we can use pose to estimate
   bearing and distance to the field object rather than the distance and bearing
   provided by the VisualFieldObject obj itself.
   @TODO  This may no longer be needed with the improvements in pose estimates
   @param c         the corner we are testing
   @param obj       the object we are testing the corner's distance to
   @return          the estimated distance between them
*/
float Context::getEstimatedDistance(const VisualCorner *c,
                                       const VisualFieldObject *obj) const
{
	const float objDist = obj->getDistance();
	const float objBearing = obj->getBearing();

    const float cornerDist = c->getDistance();
    // Convert degrees to radians for the sin/cos formulas
    const float cornerBearing = c->getBearing();

    const float dist = getEstimatedDistance(objDist, objBearing,
                                            cornerDist, cornerBearing);
    return dist;
}

/* Uses geometry to determine Distances.
   NOTE: This function looks like it should be punted.  EC 1/11/2010
   If the field object is very close (<100 cm away) we can use pose to estimate
   bearing and distance to the field object rather than the distance and bearing
   provided by the VisualFieldObject obj itself.
   @TODO  This may no longer be needed with the improvements in pose estimates
   @param c         the corner we are testing
   @param obj       the object we are testing the corner's distance to
   @return          the estimated distance between them
*/
float Context::getEstimatedDistance(const float dist1, const float bearing1,
                                 const float dist2, const float bearing2) const
{
    const float x1 = dist1 * sin(bearing1);
    const float y1 = dist1 * cos(bearing1);

    const float x2 = dist2 * sin(bearing2);
    const float y2 = dist2 * cos(bearing2);

    return Utility::getLength(x1, y1,
                              x2, y2);
}

/* Another questionable distance function from the old code.
   If the field object is very close (<100 cm away) we can use pose to estimate
   bearing and distance to the field object rather than the distance and bearing
   provided by the VisualFieldObject obj itself.
   @TODO  This may no longer be needed with the improvements in pose estimates
   @param c         the corner we are testing
   @param obj       the object we are testing the corner's distance to
   @return          the estimated distance between them
*/
float Context::getEstimatedDistance(const VisualCorner& corner,
                                       const point<int>& p) const
{
    const estimate pixEst = vision->pose->pixEstimate(p.x, p.y, 0);

    return getEstimatedDistance(pixEst.dist, pixEst.bearing,
                                corner.getDistance(), corner.getBearing());
}

/* This method should only be called with a VisualFieldObject object that has a
   concrete location on the field (YGLP, YGRP, BGLP, BGRP, BY, and YB).
   Calculates the length of the straight line between the two objects on the
   field
   @param c       A concrete (real) corner
   @param obj     A goal post
   @param which   Which goal post (if it is abstract)
   @return        floating point distance estimate
*/
float Context::getRealDistance(const ConcreteCorner *c,
                                  const VisualFieldObject *obj, int which) const
{
	if (which == 0) {
		return Utility::getLength(c->getFieldX(), c->getFieldY(),
								  obj->getFieldX(), obj->getFieldY());
	}
	return Utility::getLength(c->getFieldX(), c->getFieldY(),
								  obj->getFieldX2(), obj->getFieldY2());
}

/* Debugging method to print all of the current corner possibilities
   @param list       list of possibilities
 */
void Context::printPossibilities(const list <const ConcreteCorner*> &_list)
    const
{
    cout << "Possibilities: " << endl;
    for (list<const ConcreteCorner*>::const_iterator i = _list.begin();
         i != _list.end(); ++i) {
        cout << (*i)->toString() << endl;
    }
}

/* Creates a vector of all those field objects that are visible in the frame
   and have SURE certainty.
   @return   vector of goal posts that we have positively IDd.
*/
vector <const VisualFieldObject*> Context::getVisibleFieldObjects()
{
    vector <const VisualFieldObject*> visibleObjects;
    for (int i = 0; i < NUM_FIELD_OBJECTS_WITH_DIST_INFO; ++i) {
        if (allFieldObjects[i]->getDistance() > 0 &&
            // We don't want to identify corners based on posts that aren't sure
            allFieldObjects[i]->getIDCertainty() == _SURE) {
            // set field half information
            if (allFieldObjects[i]->getDistance() < MIDFIELD_X) {
                sameHalf = true;
            }
            objectRightX = allFieldObjects[i]->getRightBottomX();
            objectRightY = allFieldObjects[i]->getRightBottomY();
            // With the Nao we need to make sure that the goal posts are near
            // the green of the field in order to use them for distance
            if (ConcreteFieldObject::isGoal(allFieldObjects[i]->getID()) &&
                !(goalSuitableForPixEstimate(allFieldObjects[i]))) {
                // We can't use it.
            }
            else {
                visibleObjects.push_back(allFieldObjects[i]);
            }
        } else if (allFieldObjects[i]->getDistance() == 0 &&
                   allFieldObjects[i]->getIDCertainty() == _SURE) {
            // we see a post, but it is probably too close to get
            // a good distance (occluded on two sides)
            // we may not want to use the object too much, but it can help
            sameHalf = true;
            objectRightX = allFieldObjects[i]->getRightBottomX();
            objectRightY = allFieldObjects[i]->getRightBottomY();
        }
    }
    return visibleObjects;
}

/* Returns all of the field objects, regardless of their certainty.
   @return   a vector of all goal posts
 */
vector<const VisualFieldObject*> Context::getAllVisibleFieldObjects() const
{
    vector <const VisualFieldObject*> visibleObjects;
    for (int i = 0; i < NUM_FIELD_OBJECTS_WITH_DIST_INFO; ++i) {
        if (allFieldObjects[i]->getDistance() > 0){
                visibleObjects.push_back(allFieldObjects[i]);
        }
    }
    return visibleObjects;
}

/* In some Nao frames, robots obscure part of the goal and the bottom is not
   visible.  We can only use pix estimates of goals whose bottoms are visible
   @param goal       a goal we'd like to check distances to
   @return           whether or not it is a good idea
*/
const bool Context::goalSuitableForPixEstimate(const VisualFieldObject * goal)
    const
{
    // When we're the goalie, we don't expect there to be any robots blocking
    // the goal.  Furthermore, we're seeing it from a much different angle where
    // the green will not be at the bottom of the post
    const int MAX_PIXEL_DIFF = 10;
    int greenHorizon = vision->thresh->getVisionHorizon();
    int midBottomY = (goal->getLeftBottomY() + goal->getRightBottomY())/2;
    // higher values are lower in the image
    return greenHorizon - midBottomY < MAX_PIXEL_DIFF;
}

/*	Calculate the actual distance between two points.  Uses functions
	from NaoPose.cpp in Noggin
	@param x1	x coord of object 1
	@param y1	y coord of object 1
	@param x2	x coord of object 2
	@param y2	y coord of object 2
	@return		the distance in centimeters
 */

float Context::realDistance(int x1, int y1, int x2, int y2) {
	estimate r = vision->pose->pixEstimate(x1, y1, 0.0);
	estimate l = vision->pose->pixEstimate(x2, y2, 0.0);
	return vision->pose->getDistanceBetweenTwoObjects(l, r);
}


/* Get the actual length of a line.  VisualLine actually just calculates
   the length on the screen.  Here we need the real thing.
   @param line      a line we would like to have the length of
   @return          the length of the line in centimeters
 */
float Context::realLineDistance(boost::shared_ptr<VisualLine> line) {
    const point<int> end1 = line->getEndpoint();
    const point<int> end2 = line->getStartpoint();
    return realDistance(end1.x, end1.y, end2.x, end2.y);
}


/* Set facing information.  Initially this is based on whether we see a goal
   post or not.  If we do then we are facing one.  Eventually we may realize
   that we are facing a sideline (e.g. if we see a side T and no goal).
   Down the road we may want to make it more fine grained (e.g. facing a
   particular field corner).
 */
void Context::setFacing() {
    if (bluePost) {
        face = FACING_BLUE_GOAL;
    } else if (yellowPost) {
        face = FACING_YELLOW_GOAL;
    } else {
        face = FACING_UNKNOWN;
    }
}

/** Set the field half if possible by looking at the distance to any field
    objects.
 */
void Context::setFieldHalf() {
    if (face == FACING_UNKNOWN) {
        fieldHalf = HALF_UNKNOWN;
        return;
    }
    if ((face == FACING_BLUE_GOAL && sameHalf) ||
        (face == FACING_YELLOW_GOAL && !sameHalf)) {
        fieldHalf = HALF_BLUE;
    } else {
        fieldHalf = HALF_YELLOW;
    }
}

/**
   Print lots of useful debugging information.
*/
void Context::printContext() {
    cout << "Begining identify Corners." << endl;
    cout << "   Facing: ";
    switch (face) {
    case FACING_BLUE_GOAL:
        cout << "Blue goals " << endl;
        if (rightBluePost) {
            cout << "     We see a right blue post" << endl;
        }
        if (leftBluePost) {
            cout << "     We see a left blue post" << endl;
        }
        if (unknownBluePost) {
            cout << "     We see an unidentified blue post" << endl;
        }
        break;
    case FACING_YELLOW_GOAL:
        cout << "Yellow goals " << endl;
        if (rightYellowPost) {
            cout << "     We see a right yellow post" << endl;
        }
        if (leftYellowPost) {
            cout << "     We see a left yellow post" << endl;
        }
        if (unknownYellowPost) {
            cout << "     We see an unidentified yellow post" << endl;
        }
        break;
    case FACING_SIDELINE:
        cout << "Sideline " << endl;
        break;
    case FACING_UNKNOWN:
        cout << "Unknown " << endl;
        break;
    }
    cout << "   Field half is: ";
    switch (fieldHalf) {
    case HALF_UNKNOWN:
        cout << "unknown" << endl;
        break;
    case HALF_BLUE:
        cout << "blue side" << endl;
        break;
    case HALF_YELLOW:
        cout << "yellow side" << endl;
        break;
    default:
        break;
    }
    cout << "   We can see:" << endl;
    cout << "                " << tCorner << " T corners " << endl;
    cout << "                " << lCorner << " L corners " << endl;
    cout << "                " << cCorner << " CC corners" << endl;
    if (cCorner < 1 && seeCenterCircle) {
        cout << "    We see the center circle" << endl;
    }
    if (seeGoalBoxLines) {
        cout << "    We see goal box lines" << endl;
    }
}
