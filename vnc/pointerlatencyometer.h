/***************************************************************************
                  pointerlatencyometer.h  - measuring pointer latency
                             -------------------
    begin                : Wed Jun 30 12:04:44 CET 2002
    copyright            : (C) 2002 by Tim Jansen
    email                : tim@tjansen.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qdatetime.h>
#include <kdebug.h>

struct PointerState {
	int x, y;
	QTime timestamp;
};

class PointerLatencyOMeter {
private:
	enum { stateCapacity = 30, maximumLatency = 1000 };
	PointerState states[stateCapacity];
	int firstState, stateNum;
	float last3Latency, last20Latency;

public:
	PointerLatencyOMeter() :
		firstState(0),
		stateNum(0),
		last3Latency(125),
		last20Latency(25) {
	}
	
	void registerPointerState(int x, int y) {
		if (stateNum == stateCapacity)
			stateNum--;
		if (firstState == 0)
			firstState = stateCapacity-1;
		else
			firstState--;
		states[firstState].x = x;
		states[firstState].y = y;
		states[firstState].timestamp.start();
		stateNum++;
	}

	/* Returns true if pointer should be visible */
	bool registerLatency(int msecs) {
		last3Latency = ((last3Latency * 2.0) + msecs) / 3.0;
		last20Latency = ((last20Latency * 19.0) + msecs) / 20.0;

		if (msecs >= maximumLatency) 
			return true;
		if (last3Latency > (1000/4))
			return true;
		return last20Latency > (1000/12);
	}

	/* Returns true if pointer should be visible */
	bool handlePointerEvent(int x, int y) {
		for (int i = stateNum-1; i >= 0; i--) {
			int idx = (i+firstState) % stateCapacity;
			if ((states[idx].x != x) ||
			    (states[idx].y != y)) 
				continue;
			
			stateNum = i;
			int l = states[idx].timestamp.elapsed();
			return registerLatency((l > maximumLatency) ? maximumLatency : l);
		}
		return true;
	}

};
