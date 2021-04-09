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
//  readline.cpp
//  SkyBluetoothRcu
//

#include "readline.h"
#include "readline_p.h"

#include <QCoreApplication>
#include <QAbstractEventDispatcher>
#include <QRegularExpression>
#include <QReadWriteLock>
#include <QTextStream>
#include <QDebug>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <dlfcn.h>

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif



ReadLinePrivate *ReadLinePrivate::instance()
{
	static QReadWriteLock lock_;
	static QPointer<ReadLinePrivate> instance_ = nullptr;

	lock_.lockForRead();
	if (Q_UNLIKELY(instance_.isNull())) {

		lock_.unlock();
		lock_.lockForWrite();

		if (instance_.isNull())
			instance_ = new ReadLinePrivate(QAbstractEventDispatcher::instance());
	}

	lock_.unlock();
	return instance_.data();
}


// -----------------------------------------------------------------------------
/*!
	\internal

	Helper utility for returning a QtPrivate::QSlotObjectBase object pointing
	to the given method.

	\warning This function does no argument validation, DO NOT use this unless
	you're 100% sure the slot args will match the signal.

 */
template <typename Func1>
static QtPrivate::QSlotObjectBase *slotToObject(Func1 slot)
{
	typedef QtPrivate::FunctionPointer<Func1> SlotType;
	return new QtPrivate::QSlotObject<Func1, typename SlotType::Arguments, void>(slot);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Replacement for the standard qt message handler, we replace this when
	libreadline is running as we want to call the \c rl_on_new_line() function
	after printing the message so the correct prompt is shown.

	(For now?) we ignore the different error levels and just print the message
	as appropreate.

 */
void ReadLinePrivate::qtMessageHandler(QtMsgType type,
                                       const QMessageLogContext &context,
                                       const QString &message)
{
	Q_UNUSED(type);
	Q_UNUSED(context);

	fputs(message.toLatin1().constData(), stdout);
	fputc('\n', stdout);
	fflush(stdout);
	//printf("%s\n", message.toLatin1().constData());
	//printf("\n");


	ReadLinePrivate *readline = instance();
	if (readline && readline->m_rl_on_new_line && readline->isRunning()) {
		readline->m_rl_on_new_line();
	}
}



ReadLinePrivate::ReadLinePrivate(QObject *parent)
	: QObject(parent)
	, m_stdinListener(nullptr)
	, m_running(false)
	, m_maxCommandHelpWidth(30)
	, m_libHandle(nullptr)
	, m_rl_crlf(nullptr)
	, m_rl_on_new_line(nullptr)
	, m_rl_forced_update_display(nullptr)
	, m_rl_completion_matches(nullptr)
	, m_rl_bind_key(nullptr)
	, m_rl_callback_handler_install(nullptr)
	, m_rl_callback_read_char(nullptr)
	, m_rl_callback_handler_remove(nullptr)
	, m_add_history(nullptr)
{
	(void)dlerror();

#if defined(__APPLE__)
	m_libHandle = dlopen("libreadline.dylib", RTLD_NOW);
#else
	m_libHandle = dlopen("libreadline.so.5", RTLD_NOW);
#endif
	if (!m_libHandle) {
		qWarning("failed to open 'libreadline.so.5' (%s)", dlerror());
		return;
	}

	#define GET_RL_FUNC(f) \
		do { \
			m_ ## f = reinterpret_cast<f ## _t>(dlsym(m_libHandle, "" #f "")); \
			if (! m_ ## f) { \
				qWarning("failed to get symbol '" #f "' (%s)", dlerror()); \
				dlclose(m_libHandle); \
				m_libHandle = nullptr; \
				return; \
			} \
	} while(0)

	// GET_RL_FUNC(rl_crlf);
	// GET_RL_FUNC(rl_message);
	GET_RL_FUNC(rl_on_new_line);
	GET_RL_FUNC(rl_forced_update_display);
	GET_RL_FUNC(rl_completion_matches);
	GET_RL_FUNC(rl_bind_key);
	GET_RL_FUNC(rl_callback_handler_install);
	GET_RL_FUNC(rl_callback_read_char);
	GET_RL_FUNC(rl_callback_handler_remove);

	GET_RL_FUNC(add_history);

	#undef GET_RL_FUNC


	{
		// replace the completion function pointer with our own one
		void** rl_attempted_completion_function =
			reinterpret_cast<void**>(dlsym(m_libHandle, "rl_attempted_completion_function"));
		if (rl_attempted_completion_function)
			*rl_attempted_completion_function = reinterpret_cast<void*>(_completionCallback);


		// set the tab key to be the completion trigger
		rl_command_func_t* rl_complete =
			reinterpret_cast<rl_command_func_t*>(dlsym(m_libHandle, "rl_complete"));
		if (rl_complete)
			m_rl_bind_key('\t', rl_complete);

	}


	// install a command handler for the 'quit' command
	addCommand("quit", { }, "Quit program",
	           this, slotToObject(&ReadLinePrivate::onQuitCommand));
	addCommand("exit", { }, "Quit program (same as quit)",
	           this, slotToObject(&ReadLinePrivate::onQuitCommand));

	// install a command handler for the 'help' command
	addCommand("help", { }, "Display this text",
	           this, slotToObject(&ReadLinePrivate::onHelpCommand));


	// install a notifier on stdin ... this is used to feed readline
	m_stdinListener = new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read, this);
	m_stdinListener->setEnabled(false);

	QObject::connect(m_stdinListener, &QSocketNotifier::activated,
	                 this, &ReadLinePrivate::onStdinActivated);

}

ReadLinePrivate::~ReadLinePrivate()
{
	// remove the listener on stdin
	if (m_stdinListener) {
		m_stdinListener->setEnabled(false);
		delete m_stdinListener;
	}

	// uninstall the handler
	if (m_rl_callback_handler_remove)
		m_rl_callback_handler_remove();

	// close the handle to the readline library
	if (m_libHandle) {
		dlclose(m_libHandle);
		m_libHandle = nullptr;
	}

	// clean up all the command handlers
	QMap<QString, Command>::iterator it = m_commands.begin();
	while (it != m_commands.end()) {

		const Command &command = it.value();
		if (command.slotObj)
			command.slotObj->destroyIfLastRef();

		it = m_commands.erase(it);
	}
}

bool ReadLinePrivate::isValid() const
{
	return (m_libHandle != nullptr);
}

bool ReadLinePrivate::isRunning() const
{
	return m_running;
}

// -----------------------------------------------------------------------------
/*!
	\threadsafe

	Internal method intended to only be called from ReadLine, it adds a new
	command and maps it to the \a slotObj for the given \a receiver.

 */
bool ReadLinePrivate::addCommand(const QString &name, const QStringList &args,
                                 const QString &desc, const QObject *receiver,
                                 QtPrivate::QSlotObjectBase *slotObj)
{
	QMutexLocker locker(&m_commandsLock);

	// check we don't already have this commans
	if (m_commands.contains(name)) {
		qWarning() << "already have command" << name;
		return false;
	}

	// calculate the width of the command plus args string for the help text
	int helpWidth = name.length();
	for (const QString &arg : args)
		helpWidth += 1 + arg.length();

	if (helpWidth > m_maxCommandHelpWidth)
		m_maxCommandHelpWidth = qMin<int>(helpWidth, 50);

	// add the command to the map
	Command command = { args, desc, (receiver != nullptr), receiver, slotObj };
	m_commands.insert(name, std::move(command));

	return true;
}

// -----------------------------------------------------------------------------
/*!
	\threadsafe

	Runs the command as if the user had typed it.

 */
void ReadLinePrivate::runCommand(const QString &command,
                                 const QStringList &arguments)
{
	commandExecute(command, arguments);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Static completion function for libreadline

	Generator function for command completion.  STATE lets us know whether
	to start from scratch; without any state (i.e. STATE == 0), then we
	start at the top of the list.

 */
char* ReadLinePrivate::commandGenerator(const char *text, int state)
{
	Q_UNUSED(state);

	QMutexLocker locker(&m_commandsLock);

	static QList<QByteArray> matches;

	// if this is a new word to complete, initialize now.  This includes
	// saving the length of TEXT for efficiency, and initializing the index
	// variable to 0.
	if (state == 0) {

		matches.clear();

		QMap<QString, Command>::const_iterator it = m_commands.begin();
		for (; it != m_commands.end(); ++it) {
			const QString &command = it.key();
			if (command.startsWith(text))
				matches.append(command.toLatin1());
		}

	}

	// if no names matched, then return NULL.
	if (matches.empty())
		return nullptr;

	// overwise dup then remove the match from the front of the matches list
	char *match = qstrdup(matches.first().constData());
	matches.removeFirst();

	return match;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Static callback handler for the tab completion callback.

 */
char* ReadLinePrivate::_commandGenerator(const char *text, int state)
{
	ReadLinePrivate *self = instance();
	return self->commandGenerator(text, state);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Static callback handler for the tab completion callback.

 */
char** ReadLinePrivate::_completionCallback(const char *text, int start, int end)
{
	Q_UNUSED(end);

	ReadLinePrivate *self = instance();
	char **matches = nullptr;

	// if this word is at the start of the line, then it is a command to complete.
	if (start == 0)
		matches = self->m_rl_completion_matches(text, _commandGenerator);

	return matches;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Executes the given command, called from readline callback handler.

 */
void ReadLinePrivate::commandExecute(const QString &command,
                                     const QStringList &arguments)
{
	QStringList possibleCommands;

	QMutexLocker locker(&m_commandsLock);

	QMap<QString, Command>::const_iterator cmdRef = m_commands.end();
	QMap<QString, Command>::const_iterator it = m_commands.begin();
	for (; it != m_commands.end(); ++it) {

		const QString &name = it.key();
		if (name.startsWith(command)) {

			// exact matches always work
			if (command.length() == name.length()) {
				cmdRef = it;
				break;
			}

			// check if we don't already have a match, if we do then we have
			// multiple matches and have to report an error.
			if (cmdRef == m_commands.end())
				cmdRef = it;
			else
				possibleCommands << name;
		}
	}

	if (cmdRef == m_commands.end()) {
		qWarning("%s: No such command.", command.toLatin1().constData());

	} else if (!possibleCommands.isEmpty()) {
		qWarning() << "Ambiguous command" << command
		           << "possible commands:" << cmdRef.key() << possibleCommands;

	} else {

		// copy the command info
		const Command handler = cmdRef.value();

		// unlock before calling the slot
		locker.unlock();

		if (handler.slotObj) {

			// if the receiver was destroyed, skip this part
			if (Q_LIKELY(!handler.receiver.isNull() || !handler.hasValidReceiver)) {

				// we've already checked the arguments when the command was
				// added, so can safely do the following void* casts
				void *args[2] = { 0, reinterpret_cast<void*>(const_cast<QStringList*>(&arguments)) };
				handler.slotObj->call(const_cast<QObject*>(handler.receiver.data()), args);
			}
		}
	}

}

// -----------------------------------------------------------------------------
/*!
	\internal

	Callback handler from the readline library.

 */
void ReadLinePrivate::commandLineHandler(const QString &line)
{
	// split the string up using regext to group by whitespace (this takes into
	// account quoted strings)
	QRegularExpression regex(R"regex([^\"\'\s]\S*|\".*?\"|\'.*?\')regex");
	QRegularExpressionMatchIterator it = regex.globalMatch(line);

	QStringList args;

	while (it.hasNext()) {

		// get the match and strip any leading and trailing quotes
		QString match = it.next().captured(0);

		int matchLen = match.length();
		if (matchLen <= 0)
			continue;

		if ((match[0] == QChar('\"')) && (match[matchLen - 1] == QChar('\"'))) {
			match.remove(matchLen - 1, 1);
			match.remove(0, 1);
		}
		if ((match[0] == QChar('\'')) && (match[matchLen - 1] == QChar('\''))) {
			match.remove(matchLen - 1, 1);
			match.remove(0, 1);
		}

		args += match;
	}


	if (!args.empty()) {

		// the first argument should be the command
		const QString command = args.takeFirst();

		// try and excute the command
		commandExecute(command, args);

		// add the command to the history
		m_add_history(line.toLatin1().constData());
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Callback handler from the readline library.

 */
void ReadLinePrivate::_commandLineHandler(char *line)
{
	ReadLinePrivate *self = instance();
	if (line == nullptr)
		QCoreApplication::quit();
	else
		self->commandLineHandler(QString(line));
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called when input arrives on stdin, we use this to trigger libreadline.

 */
void ReadLinePrivate::onStdinActivated(int fd)
{
	Q_ASSERT(fd == STDIN_FILENO);

	if (m_rl_callback_read_char)
		m_rl_callback_read_char();
}

// -----------------------------------------------------------------------------
/*!
	Starts the input loop by enabling the listener for input on \c stdin and
	registering a libreadline callback handler.

 */
void ReadLinePrivate::start(const QString &prompt)
{
	Q_ASSERT(m_libHandle != nullptr);
	if (!m_libHandle)
		return;

	Q_ASSERT(m_stdinListener != nullptr);
	if (!m_stdinListener)
		return;

	m_rl_callback_handler_install(prompt.toLatin1().constData(),
	                              _commandLineHandler);

	m_stdinListener->setEnabled(true);

	qInstallMessageHandler(qtMessageHandler);

	m_running = true;
}

// -----------------------------------------------------------------------------
/*!
	Stops the readline input loop by disabling the listener on \c stdin and
	removing the libreadline callback handler.

 */
void ReadLinePrivate::stop()
{
	Q_ASSERT(m_libHandle != nullptr);
	if (!m_libHandle)
		return;

	Q_ASSERT(m_stdinListener != nullptr);
	if (!m_stdinListener)
		return;

	m_running = false;

	qInstallMessageHandler(0);

	m_stdinListener->setEnabled(false);

	// uninstall the handler
	m_rl_callback_handler_remove();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called when the user types 'quit'.

 */
void ReadLinePrivate::onQuitCommand(const QStringList &args)
{
	Q_UNUSED(args);

	QCoreApplication::quit();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called when the user types 'help'.

 */
void ReadLinePrivate::onHelpCommand(const QStringList &args)
{
	Q_UNUSED(args);

	// on the first pass get the max width
	QString helpText("Available commands:\n");

	QMutexLocker locker(&m_commandsLock);

	QMap<QString, Command>::const_iterator it = m_commands.begin();
	for (; it != m_commands.end(); ++it) {

		const QString &name = it.key();
		const Command &details = it.value();

		QString command(name);
		for (const QString &arg : details.arguments) {
			command += ' ';
			command += arg;
		}

		QString line = QString("  %1 %2\n").arg(command, -m_maxCommandHelpWidth)
		                                   .arg(details.description);

		helpText += line;
	}

	qWarning() << helpText;
}








ReadLine::ReadLine(QObject *parent)
	: QObject(parent)
	, m_private(ReadLinePrivate::instance())
	, m_prompt("> ")
{
	// sanity check the private singleton instance
	if (m_private.isNull() || !m_private->isValid()) {
		qCritical("failed to get private instance");
		return;
	}
}

ReadLine::~ReadLine()
{
	if (!m_private.isNull())
		m_private->stop();
}


bool ReadLine::isValid() const
{
	return !m_private.isNull() && m_private->isValid();
}

void ReadLine::setPrompt(const QString &prompt)
{
	m_prompt = prompt;
}

QString ReadLine::prompt() const
{
	return m_prompt;
}

void ReadLine::start()
{
	Q_ASSERT(!m_private.isNull());
	m_private->start(m_prompt);
}

void ReadLine::stop()
{
	Q_ASSERT(!m_private.isNull());
	m_private->stop();
}

void ReadLine::runCommand(const QString &command, const QStringList &arguments)
{
	Q_ASSERT(!m_private.isNull());
	m_private->runCommand(command, arguments);
}

bool ReadLine::addCommandImpl(const QString &name, const QStringList &args,
                              const QString &desc, const QObject *receiver,
                              QtPrivate::QSlotObjectBase *slotObj)
{
	Q_ASSERT(!m_private.isNull());
	return m_private->addCommand(name, args, desc, receiver, slotObj);
}


