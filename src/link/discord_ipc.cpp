/**
 * Taiga
 * Copyright (C) 2010-2026, Eren Okka
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "discord_ipc.hpp"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QTimer>
#include <QUuid>
#include <QtEndian>

#include "base/log.hpp"
#include "base/string.hpp"

namespace discord {

namespace {

constexpr int kSocketCount = 10;   // Discord listens on the first free of discord-ipc-{0..9}
constexpr int kHeaderSize = 8;     // opcode + length, both 32-bit little-endian
constexpr int kInvalidClientId = 4000;
constexpr qint32 kMaxFrameSize = 1024 * 1024;

}  // namespace

IpcClient::IpcClient(const QString& clientId, QObject* parent, const QString& socketPrefix,
                     std::chrono::milliseconds retryInterval)
    : QObject{parent},
      clientId_{clientId},
      socketPrefix_{socketPrefix},
      socket_{new QLocalSocket{this}},
      retryTimer_{new QTimer{this}},
      handshakeTimer_{new QTimer{this}} {
  retryTimer_->setSingleShot(true);
  retryTimer_->setInterval(retryInterval);
  connect(retryTimer_, &QTimer::timeout, this, &IpcClient::connectToDiscord);

  handshakeTimer_->setSingleShot(true);
  handshakeTimer_->setInterval(retryInterval);
  connect(handshakeTimer_, &QTimer::timeout, this, [this]() {
    if (state_ != State::Handshaking) return;
    qWarning() << "Discord: no response to handshake";
    socket_->abort();
    if (state_ != State::Disconnected) onDisconnected();
  });

  connect(socket_, &QLocalSocket::connected, this, &IpcClient::onConnected);
  connect(socket_, &QLocalSocket::disconnected, this, &IpcClient::onDisconnected);
  connect(socket_, &QLocalSocket::readyRead, this, &IpcClient::readFrames);
  connect(socket_, &QLocalSocket::errorOccurred, this, [this]() {
    // Deferred, so that the socket is not reconnected from within its own error handler.
    if (state_ == State::Connecting) QTimer::singleShot(0, this, &IpcClient::tryNextSocket);
  });
}

bool IpcClient::isEnabled() const {
  return enabled_;
}

void IpcClient::setEnabled(bool enabled) {
  if (enabled_ == enabled) return;
  enabled_ = enabled;

  if (enabled_) {
    retryAllowed_ = true;
    connectToDiscord();
  } else {
    retryTimer_->stop();
    handshakeTimer_->stop();
    // Discord removes the activity when the connection is closed. Closing immediately (rather
    // than waiting for pending writes) lets the socket be reused right away if enabled again.
    socket_->abort();
    state_ = State::Disconnected;
  }
}

bool IpcClient::isReady() const {
  return state_ == State::Ready;
}

void IpcClient::setActivity(const QJsonValue& activity) {
  activity_ = activity;
  if (state_ == State::Ready) sendActivity();
}

QString IpcClient::socketName(int index) const {
  const auto name = u"%1%2"_s.arg(socketPrefix_).arg(index);
#ifdef Q_OS_WINDOWS
  return name;  // QLocalSocket prepends `\\.\pipe\`
#else
  for (const auto variable : {"XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP"}) {
    if (const auto dir = qEnvironmentVariable(variable); !dir.isEmpty()) {
      return u"%1/%2"_s.arg(dir, name);
    }
  }
  return u"/tmp/%1"_s.arg(name);
#endif
}

void IpcClient::connectToDiscord() {
  if (!enabled_ || state_ != State::Disconnected) return;

  state_ = State::Connecting;
  socketIndex_ = 0;
  socket_->connectToServer(socketName(socketIndex_));
}

void IpcClient::tryNextSocket() {
  if (state_ != State::Connecting) return;

  if (++socketIndex_ < kSocketCount) {
    socket_->connectToServer(socketName(socketIndex_));
  } else {
    state_ = State::Disconnected;
    scheduleRetry();
  }
}

void IpcClient::onConnected() {
  state_ = State::Handshaking;
  buffer_.clear();
  handshakeTimer_->start();

  const QJsonObject handshake{{"v", 1}, {"client_id", clientId_}};
  writeFrame(Opcode::Handshake, QJsonDocument{handshake}.toJson(QJsonDocument::Compact));
}

void IpcClient::onDisconnected() {
  const bool wasConnected = state_ == State::Handshaking || state_ == State::Ready;
  state_ = State::Disconnected;
  handshakeTimer_->stop();
  buffer_.clear();

  if (wasConnected) emit disconnected();
  scheduleRetry();
}

void IpcClient::scheduleRetry() {
  if (enabled_ && retryAllowed_) retryTimer_->start();
}

void IpcClient::readFrames() {
  buffer_ += socket_->readAll();

  while (buffer_.size() >= kHeaderSize) {
    const auto opcode = qFromLittleEndian<qint32>(buffer_.constData());
    const auto length = qFromLittleEndian<qint32>(buffer_.constData() + 4);
    if (length < 0 || length > kMaxFrameSize) {
      qWarning() << "Discord: invalid frame length" << length;
      socket_->abort();
      if (state_ != State::Disconnected) onDisconnected();
      return;
    }
    if (buffer_.size() < kHeaderSize + length) break;

    const auto payload = buffer_.mid(kHeaderSize, length);
    buffer_.remove(0, kHeaderSize + length);
    handleFrame(static_cast<Opcode>(opcode), payload);
  }
}

void IpcClient::handleFrame(Opcode opcode, const QByteArray& payload) {
  const auto json = QJsonDocument::fromJson(payload).object();

  switch (opcode) {
    case Opcode::Frame: {
      const auto event = json["evt"].toString();
      if (json["cmd"].toString() == u"DISPATCH" && event == u"READY") {
        state_ = State::Ready;
        handshakeTimer_->stop();
        emit ready();
        if (activity_.isObject()) sendActivity();
      } else if (event == u"ERROR") {
        const auto data = json["data"].toObject();
        const auto message = data["message"].toString();
        qWarning() << "Discord: error" << data["code"].toInt() << message;
        emit activityRejected(data["code"].toInt(), message);
      }
      break;
    }
    case Opcode::Close:
      qWarning() << "Discord: connection closed" << json["code"].toInt()
                 << json["message"].toString();
      if (json["code"].toInt() == kInvalidClientId) retryAllowed_ = false;
      socket_->disconnectFromServer();
      break;
    case Opcode::Ping:
      writeFrame(Opcode::Pong, payload);
      break;
    default:
      break;
  }
}

void IpcClient::writeFrame(Opcode opcode, const QByteArray& payload) {
  QByteArray frame(kHeaderSize, Qt::Uninitialized);
  qToLittleEndian<qint32>(static_cast<qint32>(opcode), frame.data());
  qToLittleEndian<qint32>(static_cast<qint32>(payload.size()), frame.data() + 4);
  frame += payload;
  socket_->write(frame);
}

void IpcClient::sendActivity() {
  const QJsonObject command{
      {"cmd", "SET_ACTIVITY"},
      {"args",
       QJsonObject{
           {"pid", QCoreApplication::applicationPid()},
           {"activity", activity_},
       }},
      {"nonce", QUuid::createUuid().toString(QUuid::WithoutBraces)},
  };
  writeFrame(Opcode::Frame, QJsonDocument{command}.toJson(QJsonDocument::Compact));
}

}  // namespace discord
