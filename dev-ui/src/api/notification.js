import store from '../store'

export function notification (message, type, stay) {
  return new Promise(resolve => {
    store.dispatch({type: `notification/${type || 'default'}`, message: message, stay: Boolean(stay)})
    if (!stay) {
      setTimeout(() => {
        store.dispatch({type: 'notification/hide'})
      }, 4000)
    }
  })
}

export function success (message, stay) {
  return notification(message, 'success', stay)
}

export function error (message, stay) {
  return notification(message, 'error', stay)
}

export function reducer (state = {message: false, type: false, hidden: true, stay: false}, action) {
  let newState
  switch (action.type) {
    case 'notification/info':
    case 'notification/success':
    case 'notification/error':
    case 'notification/default':
      return {
        hidden: false,
        message: action.message,
        type: action.type.replace('notification/', ''),
        stay: action.stay
      }
    case 'notification/hide':
      newState = Object.assign({}, state)
      newState.hidden = true
      return newState
    default:
      return state
  }
}
