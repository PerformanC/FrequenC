/**
 * (PerformanC's) The Tester v2 - Deadly precise testing.
 * 
 * This file is a NOT part of FrequenC's (GitHub PerformanC/Frequenc)
 *   testing suite. You're free to use this file in your own projects, as
 *   long you don't claim it as your own, doesn't sell as a standalone product
 *   and doesn't remove this comment.
 */

import { makeRequest } from './utils.js'

const URL = 'http://127.0.0.1'
const PORT = 8888
const PATH = '/version'
const BODY = null

const TOTAL_REQUESTS = 1000

const start = performance.now()

for (let i = 0; i < TOTAL_REQUESTS; i++) {
  await makeRequest(`${URL}:${PORT}${PATH}`, {
    method: 'GET',
    headers: {
      Authorization: 'youshallnotpass'
    },
    body: BODY
  })
}

const end = performance.now()

console.log(
`[The Tester]: The benchmark has been finished.
  - Total requests: ${TOTAL_REQUESTS}
  - Path: ${PATH}

  - Total time (milliseconds): ${(end - start)}ms
  - Average time (milliseconds): ${(end - start) / TOTAL_REQUESTS}ms
`)
