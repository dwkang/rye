/*
 * Copyright (C) 2008 Search Solution Corporation. All rights reserved by Search Solution. 
 *
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met: 
 *
 * - Redistributions of source code must retain the above copyright notice, 
 *   this list of conditions and the following disclaimer. 
 *
 * - Redistributions in binary form must reproduce the above copyright notice, 
 *   this list of conditions and the following disclaimer in the documentation 
 *   and/or other materials provided with the distribution. 
 *
 * - Neither the name of the <ORGANIZATION> nor the names of its contributors 
 *   may be used to endorse or promote products derived from this software without 
 *   specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE. 
 *
 */

package rye.jdbc.driver;

import java.sql.Connection;
import java.sql.SQLException;
import java.util.Vector;

import javax.sql.ConnectionEvent;
import javax.sql.ConnectionEventListener;
import javax.sql.PooledConnection;
import javax.sql.StatementEventListener;

import rye.jdbc.jci.JciConnection;

public class RyePooledConnection implements PooledConnection
{
    private JciConnection jciCon;
    private boolean isClosed;
    private RyeConnection internalConn;
    private RyeConnectionWrapperPooling externalConn;
    private Vector<ConnectionEventListener> eventListeners;

    protected RyePooledConnection(RyeConnection cCon)
    {
	externalConn = null;
	eventListeners = new Vector<ConnectionEventListener>();
	isClosed = false;
	jciCon = null;

	internalConn = cCon;
    }

    /*
     * javax.sql.PooledConnection interface
     */

    synchronized public Connection getConnection() throws SQLException
    {
	if (isClosed) {
	    throw RyeException.createRyeException((JciConnection) null, RyeErrorCode.ER_POOLED_CONNECTION_CLOSED, null);
	}

	if (externalConn != null) {
	    externalConn.closeConnection();
	}

	if (jciCon == null) {
	    jciCon = internalConn.getJciConnection();
	}

	externalConn = new RyeConnectionWrapperPooling(jciCon, null, null, this);
	return externalConn;
    }

    synchronized public void close() throws SQLException
    {
	if (isClosed) {
	    return;
	}
	isClosed = true;
	if (externalConn != null) {
	    externalConn.closeConnection();
	}
	if (internalConn != null) {
	    internalConn.closeConnection();
	}
	if (jciCon != null) {
	    jciCon.close();
	}
	eventListeners.clear();
    }

    synchronized public void addConnectionEventListener(ConnectionEventListener listener)
    {
	if (isClosed) {
	    return;
	}

	eventListeners.addElement(listener);
    }

    synchronized public void removeConnectionEventListener(ConnectionEventListener listener)
    {
	if (isClosed) {
	    return;
	}

	eventListeners.removeElement(listener);
    }

    synchronized void notifyConnectionClosed()
    {
	externalConn = null;
	ConnectionEvent e = new ConnectionEvent(this);

	for (int i = 0; i < eventListeners.size(); i++) {
	    eventListeners.elementAt(i).connectionClosed(e);
	}
    }

    synchronized void notifyConnectionErrorOccurred(SQLException ex)
    {
	externalConn = null;
	ConnectionEvent e = new ConnectionEvent(this, ex);

	for (int i = 0; i < eventListeners.size(); i++) {
	    eventListeners.elementAt(i).connectionErrorOccurred(e);
	}
    }

    public void addStatementEventListener(StatementEventListener listener)
    {
	throw new java.lang.UnsupportedOperationException();
    }

    public void removeStatementEventListener(StatementEventListener listener)
    {
	throw new java.lang.UnsupportedOperationException();
    }
}
