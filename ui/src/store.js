import { createStore, combineReducers } from 'redux'
import { reducer as form } from 'redux-form'

// App Reducers
// import { reducer as user } from './api/user'
import { IoTClient } from './api/iot'
const iot = IoTClient.reducer
// import { reducer as notification } from './api/notification'

// Create Store
var store = createStore(
  combineReducers({
    // user,
    iot,
    // notification,
    form
  }),
  window.__REDUX_DEVTOOLS_EXTENSION__ && process.env.NODE_ENV === 'development' ? window.__REDUX_DEVTOOLS_EXTENSION__() : undefined
)

export default store
