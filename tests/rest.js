import Tester from './tester.js'

const HOST = '127.0.0.1'
const PORT = 8888
const PASSWORD = 'youshallnotpass'
const SECURE = false

const VERSION = '2.0.0'
const BRANCH = 'main'
const COMMIT = 'unknown'
const COMMIT_TIME = -1
const SOURCE_MANAGERS = []
const FILTERS = []
const PLUGINS = []

const ENCODED_TRACK = 'QAAAUwMABXRpdGxlAAZhdXRob3IAAAAAB1vNFQAKaWRlbnRpZmllcgABAAN1cmkBAAphcnR3b3JrVXJsAQAEaXNyYwAKc291cmNlTmFtZQAAAAAHW80V'
const DECODED_TRACK = {
  encoded: ENCODED_TRACK,
  info: {
    title: 'title',
    author: 'author',
    length: 123456789,
    identifier: 'identifier',
    isStream: false,
    uri: 'uri',
    artworkUrl: 'artworkUrl',
    isrc: 'isrc',
    sourceName: 'sourceName',
    position: 123456789
  }
}

const ENCODED_TRACK_ENFORCED = encodeURIComponent('QAAAPAMABXRpdGxlAAZhdXRob3IAAAAAB1vNFQAKaWRlbnRpZmllcgAAAAAACnNvdXJjZU5hbWUAAAAAB1vNFQ==')
const DECODED_TRACK_ENFORCED = {
  encoded: decodeURIComponent(ENCODED_TRACK_ENFORCED),
  info: {
    title: 'title',
    author: 'author',
    length: 123456789,
    identifier: 'identifier',
    isStream: false,
    uri: null,
    artworkUrl: null,
    isrc: null,
    sourceName: 'sourceName',
    position: 123456789
  }
}

;(async () => {
  Tester.init({
    baseUrl: `${SECURE ? 'https' : 'http'}://${HOST}:${PORT}`,
    password: PASSWORD
  })

  Tester.addTest({
    name: 'version',
    path: '/version',
    permittedMethods: [ 'GET' ],
    tests: [{
      method: 'GET',
      expected: {
        statusCode: 200,
        headers: {
          'Content-Type': 'text/plain'
        },
        body: VERSION
      }
    }]
  })

  Tester.addTest({
    name: 'info',
    path: '/v1/info',
    permittedMethods: [ 'GET' ],
    tests: [{
      method: 'GET',
      expected: {
        statusCode: 200,
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({
          version: {
            semver: VERSION,
            major: Number(VERSION.split('.')[0]),
            minor: Number(VERSION.split('.')[1]),
            patch: Number(VERSION.split('.')[2]),
            preRelease: null
          },
          builtTime: -1,
          git: {
            branch: BRANCH,
            commit: COMMIT,
            commitTime: COMMIT_TIME
          },
          isFrequenC: true,
          sourceManagers: SOURCE_MANAGERS,
          filters: FILTERS,
          plugins: PLUGINS
        }),
        type: 'json'
      }
    }]
  })

  Tester.addTest({
    name: 'decodedTrack',
    path: '/v1/decodetrack',
    permittedMethods: [ 'GET' ],
    tests: [{
      query: `?encodedTrack=${ENCODED_TRACK}`,
      method: 'GET',
      expected: {
        statusCode: 200,
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify(DECODED_TRACK),
      }
    }, {
      query: `?encodedTrack=${ENCODED_TRACK_ENFORCED}`,
      method: 'GET',
      expected: {
        statusCode: 200,
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify(DECODED_TRACK_ENFORCED),
      }
    }, {
      query: '?encodedTrack=',
      method: 'GET',
      expected: {
        statusCode: 400
      }
    }, {
      query: '?encodedTrack=AAAAA',
      method: 'GET',
      expected: {
        statusCode: 400
      }
    }]
  })

  Tester.addTest({
    name: 'decodedTracks',
    path: '/v1/decodetracks',
    permittedMethods: [ 'POST' ],
    tests: [{
      method: 'POST',
      body: 'Invalid JSON',
      expected: {
        statusCode: 400
      }
    }, {
      method: 'POST',
      body: JSON.stringify([ ENCODED_TRACK ]),
      expected: {
        statusCode: 200,
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify([ DECODED_TRACK.info ])
      }
    }, {
      method: 'POST',
      body: JSON.stringify([ ENCODED_TRACK, decodeURIComponent(ENCODED_TRACK_ENFORCED) ]),
      expected: {
        statusCode: 200,
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify([ DECODED_TRACK.info, DECODED_TRACK_ENFORCED.info ])
      }
    }, {
      method: 'POST',
      body: JSON.stringify([]),
      expected: {
        statusCode: 200,
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify([])
      }
    }]
  })

  Tester.addTest({
    name: 'encodedTracks',
    path: '/v1/encodetracks',
    permittedMethods: [ 'POST' ],
    tests: [{
      method: 'POST',
      body: 'Invalid JSON',
      expected: {
        statusCode: 400
      }
    }, {
      method: 'POST',
      body: JSON.stringify([ DECODED_TRACK.info ]),
      expected: {
        statusCode: 200,
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify([ ENCODED_TRACK ])
      }
    }, {
      method: 'POST',
      body: JSON.stringify([ DECODED_TRACK.info, DECODED_TRACK_ENFORCED.info ]),
      expected: {
        statusCode: 200,
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify([ ENCODED_TRACK, decodeURIComponent(ENCODED_TRACK_ENFORCED) ])
      }
    }, {
      method: 'POST',
      body: JSON.stringify([]),
      expected: {
        statusCode: 200,
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify([])
      }
    }]
  })

  Tester.addTest({
    name: 'encodedTrack',
    path: '/v1/encodetrack',
    permittedMethods: [ 'POST' ],
    tests: [{
      method: 'POST',
      body: 'Invalid JSON',
      expected: {
        statusCode: 400
      }
    }, {
      method: 'POST',
      body: JSON.stringify([]),
      expected: {
        statusCode: 400
      }
    }, {
      method: 'POST',
      body: JSON.stringify({}),
      expected: {
        statusCode: 400
      }
    }, {
      method: 'POST',
      body: JSON.stringify({
        ...DECODED_TRACK,
        title: undefined
      }),
      expected: {
        statusCode: 400
      }
    }, {
      method: 'POST',
      body: JSON.stringify(DECODED_TRACK.info),
      expected: {
        statusCode: 200,
        headers: {
          'Content-Type': 'text/plain'
        },
        body: ENCODED_TRACK
      }
    }, {
      method: 'POST',
      body: JSON.stringify(DECODED_TRACK_ENFORCED.info),
      expected: {
        statusCode: 200,
        headers: {
          'Content-Type': 'text/plain'
        },
        body: decodeURIComponent(ENCODED_TRACK_ENFORCED)
      }
    }]
  })

  Tester.addTest({
    name: 'loadTracks',
    path: '/v1/loadtracks',
    permittedMethods: [ 'GET' ],
    tests: [{
      method: 'GET',
      query: `?identifier=ytsearch:This+shouldnt+be+searchable+on+YouTube+${Math.random() * (10 ** 10)}`,
      expected: {
        statusCode: 200,
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({
          loadType: 'empty'
        })
      }
    }, {
      method: 'GET',
      query: `?identifier=ytsearch:Me+at+the+zoo`,
      expected: {
        statusCode: 200,
        headers: {
          'Content-Type': 'application/json'
        },
        body: ((obj) => {
          try {
            obj = JSON.parse(obj)
          } catch (e) {
            return [ 'A valid JSON', obj ]
          }

          if (typeof obj !== 'object' || obj === null) return [ 'object', typeof obj ]
          
          if (obj.loadType !== 'search') return [ 'search', obj.loadType ]

          if (typeof obj.data !== 'object') return [ 'object', typeof obj.data ]

          if (obj.data.length === 0) return [ 0, obj.data.length ]

          if (typeof obj.data[0] !== 'object' || obj.data[0] === null)
            return [ 'object', typeof obj.data[0] ]
          
          if (obj.data[0].encoded !== 'QAAApAMADU1lIGF0IHRoZSB6b28ABWphd2VkAAAAAAAASjgAC2pOUVhBQzlJVlJ3AAEAK2h0dHBzOi8vd3d3LnlvdXR1YmUuY29tL3dhdGNoP3Y9ak5RWEFDOUlWUncBADRodHRwczovL2kueXRpbWcuY29tL3ZpL2pOUVhBQzlJVlJ3L21heHJlc2RlZmF1bHQuanBnAAAHWW91VHViZQAAAAAAAAAA')
            return [ 'QAAApAMADU1lIGF0IHRoZSB6b28ABWphd2VkAAAAAAAASjgAC2pOUVhBQzlJVlJ3AAEAK2h0dHBzOi8vd3d3LnlvdXR1YmUuY29tL3dhdGNoP3Y9ak5RWEFDOUlWUncBADRodHRwczovL2kueXRpbWcuY29tL3ZpL2pOUVhBQzlJVlJ3L21heHJlc2RlZmF1bHQuanBnAAAHWW91VHViZQAAAAAAAAAA', obj.data[0].encoded ]

          if (obj.data[0].info.title !== 'Me at the zoo')
            return [ 'Me at the zoo', obj.data[0].info.title ]

          return null
        })
      }
    }]
  })

  Tester.run()
})()
