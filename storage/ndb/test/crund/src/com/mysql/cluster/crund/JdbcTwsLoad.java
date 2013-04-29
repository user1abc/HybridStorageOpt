/*
  Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package com.mysql.cluster.crund;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.SQLException;
import java.sql.PreparedStatement;
import java.sql.ResultSet;

class JdbcTwsLoad extends TwsLoad {

    // JDBC settings
    protected String jdbcDriver;
    protected String url;
    protected String username;
    protected String password;

    // JDBC resources
    protected Class jdbcDriverClass;
    protected Connection connection;
    protected PreparedStatement ins0;
    protected PreparedStatement sel0;
    protected PreparedStatement upd0;
    protected PreparedStatement del0;
    protected PreparedStatement delAll;

    public JdbcTwsLoad(TwsDriver driver) {
        super(driver);
    }

    // ----------------------------------------------------------------------
    // JDBC intializers/finalizers
    // ----------------------------------------------------------------------

    protected void initProperties() {
        out.println();
        out.print("setting jdbc properties ...");

        final StringBuilder msg = new StringBuilder();
        final String eol = System.getProperty("line.separator");

        // load the JDBC driver class
        jdbcDriver = driver.props.getProperty("jdbc.driver");
        if (jdbcDriver == null) {
            throw new RuntimeException("Missing property: jdbc.driver");
        }
        try {
            Class.forName(jdbcDriver);
        } catch (ClassNotFoundException e) {
            out.println("Cannot load JDBC driver '" + jdbcDriver
                        + "' from classpath '"
                        + System.getProperty("java.class.path") + "'");
            throw new RuntimeException(e);
        }

        url = driver.props.getProperty("jdbc.url");
        if (url == null) {
            throw new RuntimeException("Missing property: jdbc.url");
        }

        username = driver.props.getProperty("jdbc.user");
        password = driver.props.getProperty("jdbc.password");

        if (msg.length() == 0) {
            out.println("     [ok]");
        } else {
            out.println();
            out.print(msg.toString());
        }

        // have url initialized first
        descr = "jdbc(" + url + ")";
     }

    protected void printProperties() {
        out.println("jdbc.driver:                    " + jdbcDriver);
        out.println("jdbc.url:                       " + url);
        out.println("jdbc.user:                      \"" + username + "\"");
        out.println("jdbc.password:                  \"" + password + "\"");
    }

    public void init() throws Exception {
        super.init();
        assert (jdbcDriverClass == null);

        // load the JDBC driver class
        out.print("loading jdbc driver ...");
        out.flush();
        try {
            jdbcDriverClass = Class.forName(jdbcDriver);
        } catch (ClassNotFoundException e) {
            out.println("Cannot load JDBC driver '" + jdbcDriver
                        + "' from classpath '"
                        + System.getProperty("java.class.path") + "'");
            throw new RuntimeException(e);
        }
        out.println("         [ok: " + jdbcDriverClass.getName() + "]");
    }

    public void close() throws Exception {
        assert (jdbcDriverClass != null);

        //out.println();
        jdbcDriverClass = null;

        super.close();
    }

    // ----------------------------------------------------------------------
    // JDBC datastore operations
    // ----------------------------------------------------------------------

    public void initConnection() throws SQLException {
        assert (jdbcDriverClass != null);
        assert (connection == null);

        out.println();
        out.println("initializing jdbc resources ...");

        // create a connection to the database
        out.print("starting jdbc connection ...");
        out.flush();
        try {
            connection = DriverManager.getConnection(url, username, password);
        } catch (SQLException e) {
            out.println("Cannot connect to database '" + url + "'");
            throw new RuntimeException(e);
        }
        out.println("    [ok: " + url + "]");

        out.print("setting isolation level ...");
        out.flush();
        // ndb storage engine only supports READ_COMMITTED
        final int il = Connection.TRANSACTION_READ_COMMITTED;
        connection.setTransactionIsolation(il);
        out.print("     [ok: ");
        switch (connection.getTransactionIsolation()) {
        case Connection.TRANSACTION_READ_UNCOMMITTED:
            out.print("READ_UNCOMMITTED");
            break;
        case Connection.TRANSACTION_READ_COMMITTED:
            out.print("READ_COMMITTED");
            break;
        case Connection.TRANSACTION_REPEATABLE_READ:
            out.print("REPEATABLE_READ");
            break;
        case Connection.TRANSACTION_SERIALIZABLE:
            out.print("SERIALIZABLE");
            break;
        default:
            assert false;
        }
        out.println("]");

        initPreparedStatements();
    }

    public void closeConnection() throws SQLException {
        assert (connection != null);

        out.println();
        out.println("releasing jdbc resources ...");

        closePreparedStatements();

        out.print("closing jdbc connection ...");
        out.flush();
        connection.close();
        connection = null;
        out.println("     [ok]");
    }

    public void initPreparedStatements() throws SQLException {
        assert (connection != null);
        assert (ins0 == null);
        assert (sel0 == null);
        assert (upd0 == null);
        assert (del0 == null);

        out.print("using lock mode for reads ...");
        out.flush();
        final String lm;
        switch (driver.lockMode) {
        case READ_COMMITTED:
            lm = "";
            break;
        case SHARED:
            lm = " LOCK IN share mode";
            break;
        case EXCLUSIVE:
            lm = " FOR UPDATE";
            break;
        default:
            lm = "";
            assert false;
        }
        out.println("   [ok: " + "SELECT" + lm + ";]");

        out.print("compiling jdbc statements ...");
        out.flush();

        final String sqlIns0 = "INSERT INTO mytable (c0, c1, c2, c3, c5, c6, c7, c8) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
        ins0 = connection.prepareStatement(sqlIns0);

        final String sqlSel0 = ("SELECT * FROM mytable where c0=?" + lm);
        sel0 = connection.prepareStatement(sqlSel0);

        final String sqlUpd0 = "UPDATE mytable SET c1 = ?, c2 = ?, c3 = ?, c5 = ?, c6 = ?, c7 = ?, c8 = ? WHERE c0=?";
        upd0 = connection.prepareStatement(sqlUpd0);

        final String sqlDel0 = "DELETE FROM mytable WHERE c0=?";
        del0 = connection.prepareStatement(sqlDel0);

        delAll = connection.prepareStatement("DELETE FROM mytable");

        out.println("   [ok]");
    }

    protected void closePreparedStatements() throws SQLException {
        assert (ins0 != null);
        assert (sel0 != null);
        assert (upd0 != null);
        assert (del0 != null);
        assert (delAll != null);

        out.print("closing jdbc statements ...");
        out.flush();

        ins0.close();
        ins0 = null;

        sel0.close();
        sel0 = null;

        upd0.close();
        upd0 = null;

        del0.close();
        del0 = null;

        delAll.close();
        delAll = null;

        out.println("     [ok]");
    }

    // ----------------------------------------------------------------------

    public void runOperations() throws SQLException {
        out.println();
        out.println("running JDBC operations ..."
                    + "     [nRows=" + driver.nRows + "]");

        if (driver.doBulk) {
            if (driver.doInsert) runJdbcInsert(TwsDriver.XMode.BULK);
            //if (driver.doLookup) runJdbcLookup(TwsDriver.XMode.BULK);
            if (driver.doUpdate) runJdbcUpdate(TwsDriver.XMode.BULK);
            if (driver.doDelete) runJdbcDelete(TwsDriver.XMode.BULK);
        }
        if (driver.doEach) {
            if (driver.doInsert) runJdbcInsert(TwsDriver.XMode.EACH);
            if (driver.doLookup) runJdbcLookup(TwsDriver.XMode.EACH);
            if (driver.doUpdate) runJdbcUpdate(TwsDriver.XMode.EACH);
            if (driver.doDelete) runJdbcDelete(TwsDriver.XMode.EACH);
        }
        if (driver.doIndy) {
            if (driver.doInsert) runJdbcInsert(TwsDriver.XMode.INDY);
            if (driver.doLookup) runJdbcLookup(TwsDriver.XMode.INDY);
            if (driver.doUpdate) runJdbcUpdate(TwsDriver.XMode.INDY);
            if (driver.doDelete) runJdbcDelete(TwsDriver.XMode.INDY);
        }
    }

    // ----------------------------------------------------------------------

    protected void runJdbcInsert(TwsDriver.XMode mode) throws SQLException {
        final String name = "insert_" + mode.toString().toLowerCase();
        driver.beginOp(name);

        connection.setAutoCommit(mode == TwsDriver.XMode.INDY);
        for(int i = 0; i < driver.nRows; i++) {
            jdbcInsert(i, mode);
        }
        if (mode == TwsDriver.XMode.BULK)
            ins0.executeBatch();
        if (mode != TwsDriver.XMode.INDY)
            connection.commit();

        driver.finishOp(name, driver.nRows);
    }

    protected void jdbcInsert(int c0, TwsDriver.XMode mode) {
        // include exception handling as part of jdbc pattern
        try {
            final int i = c0;
            final String str = Integer.toString(i);
            ins0.setString(1, str); // key
            ins0.setString(2, str);
            ins0.setInt(3, i);
            ins0.setInt(4, i);
            ins0.setString(5, str);
            ins0.setString(6, str);
            ins0.setString(7, str);
            ins0.setString(8, str);
            if (mode == TwsDriver.XMode.BULK) {
                ins0.addBatch();
            } else {
                int cnt = ins0.executeUpdate();
                assert (cnt == 1);
            }
        } catch (SQLException e) {
            throw new RuntimeException(e);
        }
    }

    // ----------------------------------------------------------------------

    protected void runJdbcLookup(TwsDriver.XMode mode) throws SQLException {
        assert(mode != TwsDriver.XMode.BULK);

        final String name = "lookup_" + mode.toString().toLowerCase();
        driver.beginOp(name);

        connection.setAutoCommit(mode == TwsDriver.XMode.INDY);
        for(int i = 0; i < driver.nRows; i++) {
            jdbcLookup(i);
        }
        if (mode != TwsDriver.XMode.INDY)
            connection.commit();

        driver.finishOp(name, driver.nRows);
    }

    protected void jdbcLookup(int c0) {
        // include exception handling as part of jdbc pattern
        try {
            sel0.setString(1, Integer.toString(c0)); // key
            ResultSet resultSet = sel0.executeQuery();

            if (resultSet.next()) {
                // not verifying at this time
                String ac0 = resultSet.getString(1);
                String c1 = resultSet.getString(2);
                int c2 = resultSet.getInt(3);
                int c3 = resultSet.getInt(4);
                int c4 = resultSet.getInt(5);
                String c5 = resultSet.getString(6);
                String c6 = resultSet.getString(7);
                String c7 = resultSet.getString(8);
                String c8 = resultSet.getString(9);
                String c9 = resultSet.getString(10);
                String c10 = resultSet.getString(11);
                String c11 = resultSet.getString(12);
                String c12 = resultSet.getString(13);
                String c13 = resultSet.getString(14);
                String c14 = resultSet.getString(15);
            }
            assert (!resultSet.next());

            resultSet.close();
        } catch (SQLException e) {
            throw new RuntimeException(e);
        }
    }

    // ----------------------------------------------------------------------

    protected void runJdbcUpdate(TwsDriver.XMode mode) throws SQLException {
        final String name = "update_" + mode.toString().toLowerCase();
        driver.beginOp(name);

        connection.setAutoCommit(mode == TwsDriver.XMode.INDY);
        for(int i = 0; i < driver.nRows; i++) {
            jdbcUpdate(i, mode);
        }
        if (mode == TwsDriver.XMode.BULK)
            upd0.executeBatch();
        if (mode != TwsDriver.XMode.INDY)
            connection.commit();

        driver.finishOp(name, driver.nRows);
    }

    protected void jdbcUpdate(int c0, TwsDriver.XMode mode) {
        final String str0 = Integer.toString(c0);
        final int r = -c0;
        final String str1 = Integer.toString(r);

        // include exception handling as part of jdbc pattern
        try {
            upd0.setString(1, str1);
            upd0.setInt(2, r);
            upd0.setInt(3, r);
            upd0.setString(4, str1);
            upd0.setString(5, str1);
            upd0.setString(6, str1);
            upd0.setString(7, str1);
            upd0.setString(8, str0); // key
            if (mode == TwsDriver.XMode.BULK) {
                upd0.addBatch();
            } else {
                int cnt = upd0.executeUpdate();
                assert (cnt == 1);
            }
        } catch (SQLException e) {
            throw new RuntimeException(e);
        }
    }

    // ----------------------------------------------------------------------

    protected void runJdbcDelete(TwsDriver.XMode mode) throws SQLException {
        final String name = "delete_" + mode.toString().toLowerCase();
        driver.beginOp(name);

        connection.setAutoCommit(mode == TwsDriver.XMode.INDY);
        for(int i = 0; i < driver.nRows; i++) {
            jdbcDelete(i, mode);
        }
        if (mode == TwsDriver.XMode.BULK)
            del0.executeBatch();
        if (mode != TwsDriver.XMode.INDY)
            connection.commit();

        driver.finishOp(name, driver.nRows);
    }

    protected void jdbcDelete(int c0, TwsDriver.XMode mode) {
        // include exception handling as part of jdbc pattern
        try {
            final String str = Integer.toString(c0);
            del0.setString(1, str);
            if (mode == TwsDriver.XMode.BULK) {
                del0.addBatch();
            } else {
                int cnt = del0.executeUpdate();
                assert (cnt == 1);
            }
        } catch (SQLException e) {
            throw new RuntimeException(e);
        }
    }
}
