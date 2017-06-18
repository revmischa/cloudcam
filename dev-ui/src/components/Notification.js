import React from 'react'
import { connect } from 'react-redux'

import store from '../store'

class Notification extends React.Component {
  onClose () {
    store.dispatch({type: 'notification/hide'})
  }
  render () {
    let {type, message, hidden, stay} = this.props
    if (!type) {
      return null
    }
    const classes = ['Notification', 'message']
    if (type === 'info') {
      classes.push('is-info')
    }
    if (type === 'success') {
      classes.push('is-success')
    }
    if (type === 'error') {
      classes.push('is-danger')
    }
    if (hidden) {
      classes.push('hidden')
    }
    return (<div className={classes.join(' ')}>
      <div className='message-body'>
        {message}
        {stay ? <button style={{marginLeft: 5}} onClick={this.onClose} className='delete'></button> : null}
      </div>
    </div>)
  }
}

const mapStateToProps = function (store) {
  return {
    message: store.notification.message,
    type: store.notification.type,
    hidden: store.notification.hidden,
    stay: store.notification.stay
  }
}

export default connect(mapStateToProps)(Notification)
