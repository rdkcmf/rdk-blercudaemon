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
// Created by Ben Gray on 2019-07-18.
//

#ifndef CASELESSBYTEARRAY_H
#define CASELESSBYTEARRAY_H

class CaselessByteArray : public QByteArray
{
public:

	CaselessByteArray() : QByteArray() {}
	explicit CaselessByteArray(const QByteArray &other) : QByteArray(other) {}
	CaselessByteArray(const CaselessByteArray &other) : QByteArray(other) {}
	CaselessByteArray(const char *data, int size = -1) : QByteArray(data, size) {}

	inline bool operator==(const QString &s2) const { return toLower() == s2.toLower(); }
	inline bool operator!=(const QString &s2) const { return toLower() != s2.toLower(); }
	inline bool operator<(const QString &s2) const { return toLower() < s2.toLower(); }
	inline bool operator>(const QString &s2) const { return toLower() > s2.toLower(); }
	inline bool operator<=(const QString &s2) const { return toLower() <= s2.toLower(); }
	inline bool operator>=(const QString &s2) const { return toLower() >= s2.toLower(); }

	bool contains(char c) const { return toLower().contains(tolower(c)); }
	bool contains(const char *c) const { return toLower().contains(QByteArray(c).toLower()); }
	bool contains(const QByteArray &a) const { return toLower().contains(a.toLower()); }

};

inline bool operator==(const CaselessByteArray &a1, const char *a2) { return a1.toLower() == QByteArray(a2).toLower(); }
inline bool operator==(const char *a1, const CaselessByteArray &a2) { return QByteArray(a1).toLower() == a2.toLower(); }
inline bool operator==(const CaselessByteArray &a1, const QByteArray &a2) { return a1.toLower() == a2.toLower(); }
inline bool operator==(const QByteArray &a1, const CaselessByteArray &a2) { return a1.toLower() == a2.toLower(); }
inline bool operator==(const CaselessByteArray &a1, const CaselessByteArray &a2) { return a1.toLower() == a2.toLower(); }

inline bool operator!=(const CaselessByteArray &a1, const char *a2) { return a1.toLower() != QByteArray(a2).toLower(); }
inline bool operator!=(const char *a1, const CaselessByteArray &a2) { return QByteArray(a1).toLower() != a2.toLower(); }
inline bool operator!=(const CaselessByteArray &a1, const QByteArray &a2) { return a1.toLower() != a2.toLower(); }
inline bool operator!=(const QByteArray &a1, const CaselessByteArray &a2) { return a1.toLower() != a2.toLower(); }
inline bool operator!=(const CaselessByteArray &a1, const CaselessByteArray &a2) { return a1.toLower() != a2.toLower(); }

inline bool operator<(const CaselessByteArray &a1, const char *a2) { return a1.toLower() < QByteArray(a2).toLower(); }
inline bool operator<(const char *a1, const CaselessByteArray &a2) { return QByteArray(a1).toLower() < a2.toLower(); }
inline bool operator<(const CaselessByteArray &a1, const QByteArray &a2) { return a1.toLower() < a2.toLower(); }
inline bool operator<(const QByteArray &a1, const CaselessByteArray &a2) { return a1.toLower() < a2.toLower(); }
inline bool operator<(const CaselessByteArray &a1, const CaselessByteArray &a2) { return a1.toLower() < a2.toLower(); }

inline bool operator>(const CaselessByteArray &a1, const char *a2) { return a1.toLower() > QByteArray(a2).toLower(); }
inline bool operator>(const char *a1, const CaselessByteArray &a2) { return QByteArray(a1).toLower() > a2.toLower(); }
inline bool operator>(const CaselessByteArray &a1, const QByteArray &a2) { return a1.toLower() > a2.toLower(); }
inline bool operator>(const QByteArray &a1, const CaselessByteArray &a2) { return a1.toLower() > a2.toLower(); }
inline bool operator>(const CaselessByteArray &a1, const CaselessByteArray &a2) { return a1.toLower() > a2.toLower(); }

inline bool operator<=(const CaselessByteArray &a1, const char *a2) { return a1.toLower() <= QByteArray(a2).toLower(); }
inline bool operator<=(const char *a1, const CaselessByteArray &a2) { return QByteArray(a1).toLower() <= a2.toLower(); }
inline bool operator<=(const CaselessByteArray &a1, const QByteArray &a2) { return a1.toLower() <= a2.toLower(); }
inline bool operator<=(const QByteArray &a1, const CaselessByteArray &a2) { return a1.toLower() <= a2.toLower(); }
inline bool operator<=(const CaselessByteArray &a1, const CaselessByteArray &a2) { return a1.toLower() <= a2.toLower(); }

inline bool operator>=(const CaselessByteArray &a1, const char *a2) { return a1.toLower() >= QByteArray(a2).toLower(); }
inline bool operator>=(const char *a1, const CaselessByteArray &a2) { return QByteArray(a1).toLower() >= a2.toLower(); }
inline bool operator>=(const CaselessByteArray &a1, const QByteArray &a2) { return a1.toLower() >= a2.toLower(); }
inline bool operator>=(const QByteArray &a1, const CaselessByteArray &a2) { return a1.toLower() >= a2.toLower(); }
inline bool operator>=(const CaselessByteArray &a1, const CaselessByteArray &a2) { return a1.toLower() >= a2.toLower(); }


#endif // CASELESSBYTEARRAY_H
