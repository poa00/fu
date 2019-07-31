#ifndef SERVERSERVICE_H
#define SERVERSERVICE_H

#include "../store/sqlstore.h"
#include "../models/server.h"
#include "../models/protocol.h"

class ServerService
{
    SqlStore &_store;
    QList<Protocol *> _protocols;

public:
    ServerService(SqlStore &store);

    QList<Server> getAll();
    QList<Server> getAllUploadEnabled();
    Server findById(uint id);
    void append(Server &server);
    void update(Server &server);
    void save(Server &server);
    void remove(uint id);
    void setUploadEnabled(uint id, bool enabled);
    void setOutputFormatId(uint id, uint outputFormatId);
    QList<Protocol*> getProtocols();
    Protocol* findProtocol(const QString &name);
    Uploader *createUploader(const Server &server);
};

#endif // SERVERSERVICE_H
