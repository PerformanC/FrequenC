/**
 * (PerformanC's) The Tester v2 - Deadly precise testing.
 * 
 * This file is a NOT part of FrequenC's (GitHub PerformanC/Frequenc)
 *   testing suite. You're free to use this file in your own projects, as
 *   long you don't claim it as your own, doesn't sell as a standalone product
 *   and doesn't remove this comment.
 */

import http from 'http'
import https from 'https'

export function makeRequest(url, options) {
  return new Promise(async (resolve) => {
    let data = ''

    const selectedMode = url.startsWith('https') ? https : http

    const req = selectedMode.request(url, {
      method: options.method,
      headers: {
        'User-Agent': 'FrequenCTesting/1.0.0 (https://github.com/PerformanC/FrequenC)',
        ...(options.body ? { 'Content-Type': 'application/json' } : {}),
        ...(options.headers || {})
      },
      /*
        https://github.com/nodejs/node/issues/47130

        This is a workaround for a bug in Node.js that causes the
        server to hang after a few requests. This is a known bug
        and it's being fixed. You can remove this line after the
        bug is fixed.

        This bug will still happen when the seerver responds with
        "Connection: close" header.
      */
      agent: new selectedMode.Agent({
        keepAlive: false
      })
    }, (res) => {
      res.on('data', (chunk) => (data += chunk))

      res.on('error', (error) => resolve({ error }))

      res.on('end', () => {
        resolve({
          statusCode: res.statusCode,
          headers: res.headers,
          data
        })
      })
    })

    req.on('error', (error) => resolve({ error }))

    req.end(options.body)
  })
}
