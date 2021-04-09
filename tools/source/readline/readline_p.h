/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2017-2020 Sky UK
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

//
//  readline_p.h
//  SkyBluetoothRcu
//

#ifndef READLINE_P_H
#define READLINE_P_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDebug>
#include <QMutex>
#include <QMap>
#include <QPointer>
#include <QSocketNotifier>


typedef char *rl_compentry_func_t(const char *, int);
typedef int rl_command_func_t(int, int);
typedef void rl_vcpfunc_t(char *);

typedef char** rl_completion_func_t(const char *text, int start, int end);


typedef int (*rl_message_t)(const char *, ...);
typedef int (*rl_crlf_t)(void);
typedef int (*rl_on_new_line_t)(void);
typedef int (*rl_forced_update_display_t)(void);
typedef char** (*rl_completion_matches_t)(const char *, rl_compentry_func_t *);
typedef int (*rl_bind_key_t)(int, rl_command_func_t *);
typedef void (*rl_callback_handler_install_t)(const char *, rl_vcpfunc_t *);
typedef void (*rl_callback_read_char_t)(void);
typedef void (*rl_callback_handler_remove_t)(void);

typedef void (*add_history_t)(const char *);




class ReadLinePrivate : public QObject
{
	Q_OBJECT

public:
	static ReadLinePrivate *instance();
	~ReadLinePrivate() final;

private:
	explicit ReadLinePrivate(QObject *parent = nullptr);

public:
	bool isValid() const;
	bool isRunning() const;

	void start(const QString &prompt = QStringLiteral("> "));
	void stop();

	bool addCommand(const QString &name, const QStringList &args,
	                const QString &description, const QObject *receiver,
	                QtPrivate::QSlotObjectBase *slotObj);

	void runCommand(const QString &command, const QStringList &arguments);

private slots:
	void onStdinActivated(int fd);

	void onQuitCommand(const QStringList &args);
	void onHelpCommand(const QStringList &args);

private:
	static void _commandLineHandler(char *line);
	void commandLineHandler(const QString &line);
	void commandExecute(const QString &command, const QStringList &args);

	static char** _completionCallback(const char *text, int start, int end);
	static char* _commandGenerator(const char *text, int state);
	char* commandGenerator(const char *text, int state);

private:
	static void qtMessageHandler(QtMsgType type, const QMessageLogContext &context,
	                             const QString &message);


private:
	QSocketNotifier *m_stdinListener;
	bool m_running;

private:
	struct Command {
		QStringList arguments;
		QString description;
		bool hasValidReceiver;
		QPointer<const QObject> receiver;
		QtPrivate::QSlotObjectBase *slotObj;
	};

	QMutex m_commandsLock;
	QMap<QString, Command> m_commands;

	int m_maxCommandHelpWidth;


private:
	void* m_libHandle;

	rl_crlf_t m_rl_crlf;
	rl_message_t m_rl_message;
	rl_on_new_line_t m_rl_on_new_line;
	rl_forced_update_display_t m_rl_forced_update_display;
	rl_completion_matches_t m_rl_completion_matches;
	rl_bind_key_t m_rl_bind_key;
	rl_callback_handler_install_t m_rl_callback_handler_install;
	rl_callback_read_char_t m_rl_callback_read_char;
	rl_callback_handler_remove_t m_rl_callback_handler_remove;
	add_history_t m_add_history;

};


#endif // !defined(READLINE_P_H)
