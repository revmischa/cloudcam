import { createStore, combineReducers } from 'redux'
import { reducer as form } from 'redux-form'

// App Reducers
import { reducer as user } from './api/user'
import { reducer as iot } from './api/iot'
import { reducer as notification } from './api/notification'

// Create Store
var store = createStore(
  combineReducers({
    user,
    iot,
    notification,
    form
  }),
  window.devToolsExtension && process.env.NODE_ENV === 'development' ? window.devToolsExtension() : undefined
)

export default store
