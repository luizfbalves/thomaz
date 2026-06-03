export function errorBody(message: string): { ok: false; error: string } {
  return { ok: false, error: message };
}

export function authError(message: string): { ok: false; error: string } {
  return { ok: false, error: message };
}

export function actionError(message: string): { ok: false; error: string } {
  return { ok: false, error: message };
}
