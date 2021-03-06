//
//  AddressManager.h
//  libraries/networking/src
//
//  Created by Stephen Birarda on 2014-09-10.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AddressManager_h
#define hifi_AddressManager_h

#include <QtCore/QObject>
#include <QtCore/QStack>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <DependencyManager.h>

#include "AccountManager.h"

const QString HIFI_URL_SCHEME = "hifi";
const QString DEFAULT_HIFI_ADDRESS = "hifi://entry";
const QString SANDBOX_HIFI_ADDRESS = "hifi://localhost";
const QString SANDBOX_STATUS_URL = "http://localhost:60332/status";
const QString INDEX_PATH = "/";

const QString GET_PLACE = "/api/v1/places/%1";

class AddressManager : public QObject, public Dependency {
    Q_OBJECT
    SINGLETON_DEPENDENCY
    Q_PROPERTY(bool isConnected READ isConnected)
    Q_PROPERTY(QUrl href READ currentAddress)
    Q_PROPERTY(QString protocol READ getProtocol)
    Q_PROPERTY(QString hostname READ getHost)
    Q_PROPERTY(QString pathname READ currentPath)
public:
    using PositionGetter = std::function<glm::vec3()>;
    using OrientationGetter = std::function<glm::quat()>;

    enum LookupTrigger {
        UserInput,
        Back,
        Forward,
        StartupFromSettings,
        DomainPathResponse,
        Internal
    };

    bool isConnected();
    const QString& getProtocol() { return HIFI_URL_SCHEME; };

    const QUrl currentAddress() const;
    const QString currentPath(bool withOrientation = true) const;

    const QUuid& getRootPlaceID() const { return _rootPlaceID; }
    const QString& getPlaceName() const { return _placeName; }

    const QString& getHost() const { return _host; }

    void setPositionGetter(PositionGetter positionGetter) { _positionGetter = positionGetter; }
    void setOrientationGetter(OrientationGetter orientationGetter) { _orientationGetter = orientationGetter; }

    void loadSettings(const QString& lookupString = QString());

    const QStack<QUrl>& getBackStack() const { return _backStack; }
    const QStack<QUrl>& getForwardStack() const { return _forwardStack; }

    /// determines if the local sandbox is likely running. It does not account for custom setups, and is only 
    /// intended to detect the standard local sandbox install.
    void ifLocalSandboxRunningElse(std::function<void()> localSandboxRunningDoThis,
                                   std::function<void()> localSandboxNotRunningDoThat);

public slots:
    void handleLookupString(const QString& lookupString);

    // we currently expect this to be called from NodeList once handleLookupString has been called with a path
    bool goToViewpointForPath(const QString& viewpointString, const QString& pathString)
        { return handleViewpoint(viewpointString, false, DomainPathResponse, false, pathString); }

    void goBack();
    void goForward();
    void goToLocalSandbox(LookupTrigger trigger = LookupTrigger::StartupFromSettings) { handleUrl(SANDBOX_HIFI_ADDRESS, trigger); }
    void goToEntry(LookupTrigger trigger = LookupTrigger::StartupFromSettings) { handleUrl(DEFAULT_HIFI_ADDRESS, trigger); }

    void goToUser(const QString& username);

    void storeCurrentAddress();

    void copyAddress();
    void copyPath();

signals:
    void lookupResultsFinished();
    void lookupResultIsOffline();
    void lookupResultIsNotFound();

    void possibleDomainChangeRequired(const QString& newHostname, quint16 newPort);
    void possibleDomainChangeRequiredViaICEForID(const QString& iceServerHostname, const QUuid& domainID);

    void locationChangeRequired(const glm::vec3& newPosition,
                                bool hasOrientationChange, const glm::quat& newOrientation,
                                bool shouldFaceLocation);
    void pathChangeRequired(const QString& newPath);
    void hostChanged(const QString& newHost);

    void goBackPossible(bool isPossible);
    void goForwardPossible(bool isPossible);

protected:
    AddressManager();
private slots:
    void handleAPIResponse(QNetworkReply& requestReply);
    void handleAPIError(QNetworkReply& errorReply);

private:
    void goToAddressFromObject(const QVariantMap& addressMap, const QNetworkReply& reply);

    // Set host and port, and return `true` if it was changed.
    bool setHost(const QString& host, LookupTrigger trigger, quint16 port = 0);
    bool setDomainInfo(const QString& hostname, quint16 port, LookupTrigger trigger);

    const JSONCallbackParameters& apiCallbackParameters();

    bool handleUrl(const QUrl& lookupUrl, LookupTrigger trigger = UserInput);

    bool handleNetworkAddress(const QString& lookupString, LookupTrigger trigger, bool& hostChanged);
    void handlePath(const QString& path, LookupTrigger trigger, bool wasPathOnly = false);
    bool handleViewpoint(const QString& viewpointString, bool shouldFace, LookupTrigger trigger,
                         bool definitelyPathOnly = false, const QString& pathString = QString());
    bool handleUsername(const QString& lookupString);
    bool handleDomainID(const QString& host);

    void attemptPlaceNameLookup(const QString& lookupString, const QString& overridePath, LookupTrigger trigger);
    void attemptDomainIDLookup(const QString& lookupString, const QString& overridePath, LookupTrigger trigger);

    void addCurrentAddressToHistory(LookupTrigger trigger);

    QString _host;
    quint16 _port;
    QString _placeName;
    QUuid _rootPlaceID;
    PositionGetter _positionGetter;
    OrientationGetter _orientationGetter;

    QStack<QUrl> _backStack;
    QStack<QUrl> _forwardStack;
    quint64 _lastBackPush = 0;

    QString _newHostLookupPath;
};

#endif // hifi_AddressManager_h
