/**
 * (PerformanC's) The Tester v2 - Deadly precise testing.
 * 
 * This file is a NOT part of FrequenC's (GitHub PerformanC/Frequenc)
 *   testing suite. You're free to use this file in your own projects, as
 *   long you don't claim it as your own, doesn't sell as a standalone product
 *   and doesn't remove this comment.
 */

import fs from 'fs'

import { makeRequest } from './utils.js'

const HTTP_METHODS = [ 'GET', 'POST', 'PUT', 'PATCH', 'DELETE', 'HEAD', 'OPTIONS' ]
const HTTP_STATUS_CODES = [
  100, 101, 102, 103, 104, 105, 106, 107, 108, 109,
  200, 201, 202, 203, 204, 205, 206, 207, 208, 226,
  300, 301, 302, 303, 304, 305, 306, 307, 308, 309,
  400, 401, 402, 403, 404, 405, 406, 407, 408, 409,
  410, 411, 412, 413, 414, 415, 416, 417, 418, 419,
  420, 421, 422, 423, 424, 425, 426, 428, 429, 431,
  451, 500, 501, 502, 503, 504, 505, 506, 507, 508,
  510, 511
]

const testInfo = {
  type: 'HTTP-API',
  baseUrl: null,
  password: null,
  options: {
    curlCommands: {
      enabled: false,
      fd: null
    }
  },
  tests: []
}

/**
 * Tester.init({
 *  baseUrl: 'http://localhost:3000',
 *  password: 'password',
 *  giveCURLCommands: true
 * })
 */
function init(obj) {
  if (!obj.baseUrl) throw new Error('baseUrl is required')

  if (!obj.password) throw new Error('password is required')

  if (typeof obj.giveCURLCommands !== 'boolean') throw new Error('giveCURLCommands must be a boolean')

  testInfo.baseUrl = obj.baseUrl
  testInfo.password = obj.password

  if (obj.giveCURLCommands) {
    testInfo.options.curlCommands.enabled = true
    testInfo.options.curlCommands.fd = fs.openSync(`./test-report-${process.hrtime.bigint()}.txt`, 'w')
  }

  console.log(`[The Tester]: Tester initialized with ${obj.baseUrl} and ${obj.password}`)
}

/**
 * Tester.addTest({
 *   name: 'Test name',
 *   path: '/test',
 *   permittedMethods: [ 'GET' ],
 *   disableErrorTests: false,
 *   tests: [{
 *     method: 'GET',
 *     expected: {
 *       statusCode: 200,
 *       headers: {
 *         'Content-Type': 'text/plain'
 *       },
 *       body: 'Hello, world!'
 *     }
 *   }]
 * })
 * 
 * WARN: If any field is set to null or undefined, it will
 *   be marked that it can't receive ANY value. Set to false if
 *   you don't want to check.
 * 
 * NOTE: If you want to check the body, set the type to 'json'.
 * 
 * NOTE: If you want to check the body with a function, set the
 *  type to 'function' and set the function as the body. The
 *  function will receive the actual body and should return
 *  false if it's correct and an array with the expected and
 *  actual values if it's incorrect.
 */
function addTest(obj) {
  if (!obj.name) throw new Error('name is required')
  if (typeof obj.name !== 'string') throw new Error('name must be a string')

  if (!obj.path) throw new Error('path is required')
  if (typeof obj.path !== 'string') throw new Error('path must be a string')

  if (!obj.permittedMethods) throw new Error('permittedMethods is required')
  if (!Array.isArray(obj.permittedMethods)) throw new Error('permittedMethods must be an array')

  if (typeof obj.disableErrorTests !== 'boolean') throw new Error('disableErrorTests must be a boolean')

  obj.permittedMethods.forEach((method) => {
    if (!HTTP_METHODS.includes(method)) throw new Error(`Invalid method: ${method}`)
  })

  if (!obj.tests) throw new Error('tests is required')
  if (!Array.isArray(obj.tests)) throw new Error('tests must be an array')

  obj.tests.forEach((test) => {
    if (test.query && typeof test.query !== 'string') throw new Error('query must be a string')

    if (!test.method) throw new Error('method is required')
    if (typeof test.method !== 'string') throw new Error('method must be a string')
    if (!HTTP_METHODS.includes(test.method)) throw new Error(`Invalid method: ${test.method}`)

    if (test.body && typeof test.body !== 'string') throw new Error('body must be a string')

    if (!test.expected) throw new Error('expected is required')
    if (typeof test.expected !== 'object') throw new Error('expected must be an object')

    if (!test.expected.statusCode) throw new Error('expected.statusCode is required')
    if (typeof test.expected.statusCode !== 'number') throw new Error('expected.statusCode must be a number')
    if (!HTTP_STATUS_CODES.includes(test.expected.statusCode)) throw new Error(`Invalid status code: ${test.expected.statusCode}`)

    if (test.expected.headers) {
      if (typeof test.expected.headers !== 'object') throw new Error('expected.headers must be an object')

      Object.keys(test.expected.headers).forEach((header) => {
        if (typeof header !== 'string') throw new Error('expected.headers keys must be strings')

        if (typeof test.expected.headers[header] !== 'string') throw new Error('expected.headers values must be strings')
      })
    }

    if (test.expected.body) {
      if (typeof test.expected.body !== 'string' && typeof test.expected.body !== 'boolean' && typeof test.expected.body !== 'function') throw new Error('expected.body must be a string, boolean or function')
    }

    if (test.expected.type) {
      if (test.expected.type !== 'json' && test.expected.type !== 'function') throw new Error('expected.type must be "json" or "function"')
    }
  })

  console.log(`[The Tester]: Adding test ${obj.name}.\n - Path: ${obj.path}\n - Allowed methods: ${obj.permittedMethods.map((method) => method.toUpperCase()).join(', ')}`)

  testInfo.tests.push(obj)
}

function _checkHeaders(expected, actual) {
  return new Promise((resolve) => {
    if (expected === false) resolve({ success: true })

    if (!expected && actual.length !== 0) resolve({ success: false, message: [ 'None', `${actual.length} headers received` ] })

    Object.keys(expected).forEach((header) => {
      let patchedHeader = header.toLowerCase()

      if (!actual[patchedHeader]) resolve({ success: false, message: [ header, 'Doesn\'t exist' ] })

      if (expected[header] !== actual[patchedHeader]) resolve({ success: false, message: [ expected[header], actual[patchedHeader] ] })
    })

    resolve({ success: true })
  })
}

function _checkBody(expected, actual) {
  if (typeof expected === 'function') {
    const res = expected(actual)

    if (res) return { success: false, message: res }

    return { success: true }
  }

  if (expected) {
    if (actual !== expected) return { success: false, message: [ expected, actual ] }

    return { success: true }
  } else if (expected === false) {
    if (actual) return { success: false, message: [ 'None', actual ] }

    return { success: true }
  } else if (!expected && actual) {
    return { success: false, message: [ 'None', actual ] }
  }

  return { success: true }
}

Array.prototype.nForEach = async function(callback) {
  return new Promise(async (resolve) => {
    for (let i = 0; i < this.length; i++) {
      const res = await callback(this[i], i)

      if (res) return resolve()
    }

    resolve()
  })
}

async function run() {
  console.log(`[The Tester]: Running ${testInfo.tests.length} tests...`)

  let now = Date.now()

  const tests = testInfo.tests

  let passed = 0
  let failed = 0
  let testsLength = 0

  const textResult = [ '# The Tester v2 - Results', '', '# Normal Tests', '', '' ]

  await tests.nForEach(async (test) => {
    textResult.push(`## ${test.name} - vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n\n`)

    await test.tests.nForEach(async (test2, i) => {
      testsLength++

      textResult.push(`### ${test.name} #${i + 1}`)
      textResult.push(` - Passed? __THE_TESTER_${test.name}_${i}__`)

      const cURLCommand = [ 'curl' ]

      cURLCommand.push(`"${testInfo.baseUrl}${test.path}${test2.query || ''}"`)
      cURLCommand.push('-X', test2.method.toUpperCase())
      if (test2.body) cURLCommand.push('-d', `"${test2.body}"`)
      cURLCommand.push('-H', `"Authorization: ${testInfo.password}"`)

      Object.keys(test2.expected.headers || {}).forEach((header) => {
        cURLCommand.push('-H', `"${header}: ${test2.expected.headers[header]}"`)
      })

      textResult.push(` - cURL Command: ${cURLCommand.join(' ')}`)
      textResult.push(` - Method: ${test2.method}`)
      textResult.push(` - Path: ${test.path}`)
      textResult.push(` - Query: ${test2.query || 'N/A'}`)
      textResult.push(` - Body: ${test2.body || 'N/A'}`)
      textResult.push(` - Expected status code: ${test2.expected.statusCode}\n`)

      const res = await makeRequest(`${testInfo.baseUrl}${test.path}${test2.query || ''}`, {
        headers: {
          'Authorization': testInfo.password
        },
        body: test2.body,
        method: test2.method
      })

      if (res.error) {
        console.error(`[The Tester]: Test ${test.name} #${i + 1} failed: ${res.error}`)

        failed++

        textResult.forEach((line, j) => {
          if (line !== ` - Passed? __THE_TESTER_${test.name}_${i}__`) return;

          textResult[j] = ` - Passed? No. The request failed: ${res.error}`
        })

        return false
      }

      if (test2.expected.statusCode !== res.statusCode) {
        console.error(`[The Tester]: Status code test of ${test.name} #${i + 1} failed.\n - Expected: ${test2.expected.statusCode}\n - Actual: ${res.statusCode}`)

        failed++

        textResult.forEach((line, j) => {
          if (line !== ` - Passed? __THE_TESTER_${test.name}_${i}__`) return;
  
          textResult[j] = ' - Passed? No. The status code is incorrect.'
        })

        return false
      }

      const headersResult = await _checkHeaders(test2.expected.headers || {}, res.headers)

      if (!headersResult.success) {
        console.error(`[The Tester]: Headers test of ${test.name} #${i + 1} failed.\n - Expected: ${headersResult.message[0]}\n - Actual: ${headersResult.message[1]}`)

        failed++

        textResult.forEach((line, j) => {
          if (line !== ` - Passed? __THE_TESTER_${test.name}_${i}__`) return;
  
          textResult[j] = ' - Passed? No. The headers are incorrect.'
        })

        return false
      }

      if (test2.expected.type == 'json') {
        try {
          JSON.parse(res.data)
        } catch {
          console.error(`[The Tester]: Body test of ${test.name} #${i + 1} failed.\n - Expected: JSON\n - Actual: Not JSON/Malformed JSON: ${res.data}`)

          failed++

          textResult.forEach((line, j) => {
            if (line !== ` - Passed? __THE_TESTER_${test.name}_${i}__`) return;
    
            textResult[j] = ' - Passed? No. The body is not a JSON.'
          })

          return false
        }
      }

      const bodyResult = _checkBody(test2.expected.body, res.data)

      if (!bodyResult.success) {
        console.error(`[The Tester]: Body test of ${test.name} #${i + 1} failed.\n - Expected: ${bodyResult.message[0]}\n - Actual: ${bodyResult.message[1]}`)

        failed++

        textResult.forEach((line, j) => {
          if (!line !== ` - Passed? __THE_TESTER_${test.name}_${i}__`) return;
  
          textResult[j] = ' - Passed? No. The body is incorrect.'
        })

        return false
      }

      textResult.forEach((line, j) => {
        if (line !== ` - Passed? __THE_TESTER_${test.name}_${i}__`) return;

        textResult[j] = ' - Passed? Yes.'
      })

      passed++
    })

    textResult.push(`\n ## ${test.name} - ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n\n`)
  })

  textResult.push('', '', '# Error Tests', '', '')

  console.log(`[The Tester]: ${passed} tests passed, ${failed} tests failed. ${passed / testsLength * 100}% passed. ${failed / testsLength * 100}% failed.`)

  console.log(`[The Tester]: Normal tests finished. Running error tests...`)

  passed = 0
  failed = 0
  testsLength = 0

  await tests.nForEach(async (test) => {
    if (test.disableErrorTests) return false

    textResult.push(`## ${test.name} - vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n\n`)

    await HTTP_METHODS.nForEach(async (method) => {
      if (test.permittedMethods.includes(method)) return false

      testsLength++

      textResult.push(`### ${test.name} #${method}`)
      textResult.push(` - Passed? __THE_TESTER_${test.name}_${method}__`)

      const cURLCommand = [ 'curl' ]

      cURLCommand.push(`"${testInfo.baseUrl}${test.path}"`)
      cURLCommand.push('-X', method.toUpperCase())
      cURLCommand.push('-H', `"Authorization: ${testInfo.password}"`)

      textResult.push(` - cURL Command: ${cURLCommand.join(' ')}`)
      textResult.push(` - Method: ${method}`)
      textResult.push(` - Path: ${test.path}`)
      textResult.push(` - Expected status code: 405\n`)

      const res = await makeRequest(`${testInfo.baseUrl}${test.path}`, {
        headers: {
          'Authorization': testInfo.password
        },
        method
      })

      if (res.error) {
        console.error(`[The Tester]: Test ${test.name} failed: ${res.error}`)

        failed++

        textResult.forEach((line, j) => {
          if (line !== ` - Passed? __THE_TESTER_${test.name}_${method}__`) return;

          textResult[j] = ` - Passed? No. The request failed: ${res.error}`
        })

        return false
      }

      if (res.statusCode !== 405) {
        console.error(`[The Tester]: Error test ${test.name} failed.\n - Expected: 405\n - Actual: ${res.statusCode}`)

        failed++

        textResult.forEach((line, j) => {
          if (line !== ` - Passed? __THE_TESTER_${test.name}_${method}__`) return;

          textResult[j] = ' - Passed? No. The status code is incorrect.'
        })

        return false
      }

      textResult.forEach((line, j) => {
        if (line !== ` - Passed? __THE_TESTER_${test.name}_${method}__`) return;

        textResult[j] = ' - Passed? Yes.'
      })

      passed++
    })

    textResult.push(`\n ## ${test.name} - ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n\n`)
  })

  console.log(`[The Tester]: ${passed} error tests passed, ${failed} error tests failed. ${passed / testsLength * 100}% passed. ${failed / testsLength * 100}% failed.`)

  console.log(`[The Tester]: All tests finished in ${Date.now() - now}ms.`)

  if (testInfo.options.curlCommands.enabled) {
    fs.writeSync(testInfo.options.curlCommands.fd, textResult.join('\n'))

    fs.closeSync(testInfo.options.curlCommands.fd)

    console.log(`[The Tester]: cURL commands written to test-report.txt`)
  }
}

export default {
  init,
  addTest,
  run
}