# auth.py — authentication, sessions, user management for CM5
#
# Security principles:
#   - Passwords stored as PBKDF2-SHA256 hashes only, never logged
#   - Session tokens are random 32-byte URL-safe strings, server-side only
#   - Soft deletes: active=0, data retained
#   - _db() works from any thread (fresh connection per call)

import datetime
import secrets
import sqlite3
from contextlib import contextmanager
from functools import wraps
from pathlib import Path

from flask import redirect, request, make_response
from werkzeug.security import check_password_hash, generate_password_hash

DB            = Path(__file__).parent / 'cm5.db'
COOKIE        = 'cm5_tok'
SESSION_HOURS = 8

# ── Schema ────────────────────────────────────────────────────────────────────

_SCHEMA = '''
PRAGMA journal_mode=WAL;

CREATE TABLE IF NOT EXISTS users (
    id         INTEGER PRIMARY KEY,
    username   TEXT    NOT NULL UNIQUE,
    pw_hash    TEXT    NOT NULL,
    role       TEXT    NOT NULL DEFAULT 'viewer',
    created_at TEXT    NOT NULL,
    last_login TEXT,
    active     INTEGER NOT NULL DEFAULT 1
);

CREATE TABLE IF NOT EXISTS sessions (
    id         INTEGER PRIMARY KEY,
    token      TEXT    NOT NULL UNIQUE,
    user_id    INTEGER NOT NULL REFERENCES users(id),
    ip         TEXT,
    created_at TEXT    NOT NULL,
    expires_at TEXT    NOT NULL
);

CREATE TABLE IF NOT EXISTS settings (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_sessions_tok ON sessions(token);
'''

# ── DB connection ─────────────────────────────────────────────────────────────

@contextmanager
def _db():
    conn = sqlite3.connect(str(DB), timeout=10)
    conn.row_factory = sqlite3.Row
    conn.execute('PRAGMA foreign_keys=ON')
    try:
        yield conn
        conn.commit()
    finally:
        conn.close()


def init_db():
    with _db() as db:
        db.executescript(_SCHEMA)


def needs_setup():
    with _db() as db:
        return db.execute('SELECT COUNT(*) FROM users').fetchone()[0] == 0


# ── Users ─────────────────────────────────────────────────────────────────────

def create_user(username, password, role='viewer'):
    now = _now()
    pw  = generate_password_hash(password)
    with _db() as db:
        db.execute(
            'INSERT INTO users (username, pw_hash, role, created_at) VALUES (?,?,?,?)',
            (username.strip().lower(), pw, role, now))


def authenticate(username, password):
    with _db() as db:
        row = db.execute(
            'SELECT * FROM users WHERE username=? AND active=1',
            (username.strip().lower(),)).fetchone()
    dummy = 'pbkdf2:sha256:260000$x$' + 'x' * 43
    if row:
        ok = check_password_hash(row['pw_hash'], password)
    else:
        check_password_hash(dummy, password)
        ok = False
    if ok:
        with _db() as db:
            db.execute('UPDATE users SET last_login=? WHERE id=?', (_now(), row['id']))
        return dict(row)
    return None


def get_user(user_id):
    with _db() as db:
        row = db.execute('SELECT * FROM users WHERE id=?', (user_id,)).fetchone()
        return dict(row) if row else None


def list_users():
    with _db() as db:
        return [dict(r) for r in db.execute(
            'SELECT id, username, role, created_at, last_login, active FROM users '
            'ORDER BY username').fetchall()]


def set_role(user_id, role):
    with _db() as db:
        db.execute('UPDATE users SET role=? WHERE id=?', (role, user_id))


def set_password(user_id, new_password):
    pw = generate_password_hash(new_password)
    with _db() as db:
        db.execute('UPDATE users SET pw_hash=? WHERE id=?', (pw, user_id))
    end_all_sessions(user_id)


def deactivate_user(user_id):
    end_all_sessions(user_id)
    with _db() as db:
        db.execute('UPDATE users SET active=0 WHERE id=?', (user_id,))


def activate_user(user_id):
    with _db() as db:
        db.execute('UPDATE users SET active=1 WHERE id=?', (user_id,))


# ── Sessions ──────────────────────────────────────────────────────────────────

def start_session(user_id, ip):
    token = secrets.token_urlsafe(32)
    now   = _now()
    exp   = (_utcnow() + datetime.timedelta(hours=SESSION_HOURS)).isoformat() + 'Z'
    with _db() as db:
        db.execute(
            'INSERT INTO sessions (token,user_id,ip,created_at,expires_at) VALUES (?,?,?,?,?)',
            (token, user_id, ip or '', now, exp))
    return token


def get_session(token):
    if not token:
        return None
    with _db() as db:
        row = db.execute(
            'SELECT s.id, s.expires_at, s.user_id, u.username, u.role, u.active '
            'FROM sessions s JOIN users u ON s.user_id=u.id '
            'WHERE s.token=? AND u.active=1',
            (token,)).fetchone()
    if not row:
        return None
    exp = row['expires_at'].rstrip('Z')
    try:
        if _utcnow() > datetime.datetime.fromisoformat(exp):
            end_session(token)
            return None
    except ValueError:
        end_session(token)
        return None
    return {'id': row['user_id'], 'username': row['username'], 'role': row['role']}


def end_session(token):
    with _db() as db:
        db.execute('DELETE FROM sessions WHERE token=?', (token,))


def end_all_sessions(user_id):
    with _db() as db:
        db.execute('DELETE FROM sessions WHERE user_id=?', (user_id,))


# ── Settings ──────────────────────────────────────────────────────────────────

def get_setting(key, default=''):
    with _db() as db:
        row = db.execute('SELECT value FROM settings WHERE key=?', (key,)).fetchone()
        return row['value'] if row else default


def set_setting(key, value):
    with _db() as db:
        db.execute('INSERT INTO settings(key,value) VALUES(?,?) '
                   'ON CONFLICT(key) DO UPDATE SET value=excluded.value',
                   (key, value))


# ── Auth decorators ───────────────────────────────────────────────────────────

def current_user():
    return get_session(request.cookies.get(COOKIE))


def require_auth(f):
    @wraps(f)
    def wrapped(*a, **kw):
        if current_user() is None:
            return redirect('/login')
        return f(*a, **kw)
    return wrapped


def require_admin(f):
    @wraps(f)
    def wrapped(*a, **kw):
        u = current_user()
        if u is None:
            return redirect('/login')
        if u.get('role') != 'admin':
            from flask import Response
            return Response('{"error":"Forbidden"}', status=403,
                            mimetype='application/json')
        return f(*a, **kw)
    return wrapped


def set_session_cookie(response, token):
    response.set_cookie(COOKIE, token, max_age=SESSION_HOURS * 3600,
                        httponly=True, samesite='Strict', path='/')
    return response


def clear_session_cookie(response):
    response.delete_cookie(COOKIE, path='/')
    return response


# ── Login / setup HTML ────────────────────────────────────────────────────────

_LOGIN_HTML = '''<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CM5 — Sign in</title>
<style>
  *{{box-sizing:border-box;margin:0;padding:0}}
  body{{background:#0e1629;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;
        display:flex;align-items:center;justify-content:center;min-height:100vh}}
  .card{{background:#1a2744;border-radius:10px;padding:36px 40px;width:320px;
         box-shadow:0 8px 32px rgba(0,0,0,.4)}}
  h1{{color:#00d4ff;font-size:1.2em;margin-bottom:4px;text-align:center;font-weight:600}}
  .sub{{color:rgba(255,255,255,.4);font-size:11px;text-align:center;margin-bottom:24px}}
  label{{display:block;font-size:12px;color:rgba(255,255,255,.55);margin-bottom:4px}}
  input{{width:100%;padding:9px 12px;font-size:13px;border:1px solid rgba(255,255,255,.15);
         border-radius:5px;margin-bottom:14px;background:#0e1629;color:#fff}}
  input:focus{{outline:none;border-color:#00d4ff}}
  button{{width:100%;padding:10px;background:#006BAC;color:#fff;border:none;
          border-radius:5px;font-size:14px;font-weight:600;cursor:pointer}}
  button:hover{{background:#0080cc}}
  .err{{color:#f87171;font-size:12px;text-align:center;margin-bottom:12px}}
</style>
</head>
<body>
<div class="card">
  <h1>CM5 Platform</h1>
  {site_name_html}
  {error}
  <form method="POST" action="/login" autocomplete="off">
    <label>Username</label>
    <input type="text" name="username" autofocus autocomplete="username">
    <label>Password</label>
    <input type="password" name="password" autocomplete="current-password">
    <button type="submit">Sign in</button>
  </form>
</div>
</body>
</html>'''

_SETUP_HTML = '''<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>CM5 — First-run setup</title>
<style>
  *{{box-sizing:border-box;margin:0;padding:0}}
  body{{background:#0e1629;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;
        display:flex;align-items:center;justify-content:center;min-height:100vh}}
  .card{{background:#1a2744;border-radius:10px;padding:36px 40px;width:360px;
         box-shadow:0 8px 32px rgba(0,0,0,.4)}}
  h1{{color:#00d4ff;font-size:1.2em;margin-bottom:8px}}
  p{{font-size:12px;color:rgba(255,255,255,.45);margin-bottom:20px}}
  label{{display:block;font-size:12px;color:rgba(255,255,255,.55);margin-bottom:4px}}
  input{{width:100%;padding:9px 12px;font-size:13px;border:1px solid rgba(255,255,255,.15);
         border-radius:5px;margin-bottom:14px;background:#0e1629;color:#fff}}
  input:focus{{outline:none;border-color:#00d4ff}}
  button{{width:100%;padding:10px;background:#006BAC;color:#fff;border:none;
          border-radius:5px;font-size:14px;font-weight:600;cursor:pointer}}
  .err{{color:#f87171;font-size:12px;text-align:center;margin-bottom:12px}}
</style>
</head>
<body>
<div class="card">
  <h1>Create Admin Account</h1>
  <p>No users exist. Create the initial admin account to get started.</p>
  {error}
  <form method="POST" action="/setup">
    <label>Username</label>
    <input type="text" name="username" value="admin">
    <label>Password (min 8 characters)</label>
    <input type="password" name="password" placeholder="Password">
    <label>Confirm password</label>
    <input type="password" name="confirm">
    <button type="submit">Create Admin Account</button>
  </form>
</div>
</body>
</html>'''


def login_page(error=''):
    err_html  = f'<p class="err">{error}</p>' if error else ''
    site_name = get_setting('site_name', '')
    sn_html   = (f'<p class="sub">{site_name}</p>' if site_name
                 else '<p class="sub">UTMC UTC Type 2 Outstation</p>')
    return _LOGIN_HTML.format(error=err_html, site_name_html=sn_html)


def setup_page(error=''):
    err_html = f'<p class="err">{error}</p>' if error else ''
    return _SETUP_HTML.format(error=err_html)


# ── Helpers ───────────────────────────────────────────────────────────────────

def _utcnow():
    return datetime.datetime.now(datetime.timezone.utc)

def _now():
    return _utcnow().strftime('%Y-%m-%dT%H:%M:%SZ')
