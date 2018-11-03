/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Interface FilterActionFactory.
 *
 * Created: David Williams <djw@netscape.com>,  1 Oct 1997.
 */

package grendel.filters;

import java.io.IOException;

import javax.mail.Message;

import grendel.filters.IFilterAction;
import grendel.filters.FilterRulesParser;
import grendel.filters.FilterSyntaxException;

public interface IFilterActionFactory {
  public IFilterAction Make(FilterRulesParser p)
        throws IOException, FilterSyntaxException;
  public IFilterAction Make(String[] args) throws FilterSyntaxException;
  public String getName();
  public String toString(); // should be implimented
}
