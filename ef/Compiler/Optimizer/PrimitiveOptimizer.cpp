/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "PrimitiveOptimizer.h"
#include "ControlGraph.h"

//
// Quickly trim primitives whose results are not used from the graph.
// This is a quick-and-dirty implementation that misses some opportunities;
// for example, cycles of dead primitives are not detected.
//
void simpleTrimDeadPrimitives(ControlGraph &cg)
{
	cg.dfsSearch();
	ControlNode **dfsList = cg.dfsList;
	ControlNode **pcn = dfsList + cg.nNodes;

	while (pcn != dfsList) {
		ControlNode &cn = **--pcn;

		// Search the primitives backwards; remove any whose outputs are not used
		// and which are not roots.
		DoublyLinkedList<Primitive> &primitives = cn.getPrimitives();
		DoublyLinkedList<Primitive>::iterator primIter = primitives.end();
		while (!primitives.done(primIter)) {
			Primitive &p = primitives.get(primIter);
			primIter = primitives.retreat(primIter);
			if (!p.isRoot() && !p.hasConsumers())
				p.destroy();
		}

		// Search the phi nodes backwards; remove any whose outputs are not used.
		DoublyLinkedList<PhiNode> &phiNodes = cn.getPhiNodes();
		DoublyLinkedList<PhiNode>::iterator phiIter = phiNodes.end();
		while (!phiNodes.done(phiIter)) {
			PhiNode &n = phiNodes.get(phiIter);
			phiIter = phiNodes.retreat(phiIter);
			if (!n.hasConsumers())
				n.remove();
		}
	}
}

