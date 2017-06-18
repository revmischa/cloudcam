import React from 'react'
import { Link, browserHistory } from 'react-router'

import { login } from '../api/user'
import { error, success } from '../api/notification'
import FormLogin from '../components/FormLogin'

export default class Login extends React.Component {
  onSubmit (form) {
    return login(form)
      .then(data => {
        browserHistory.push('/')
        success('Logged in.')
      })
      .catch(e => {
        error(e && e.message ? e.message : 'Could not login.')
      })
  }

  render () {
    return (
      <div className='section form is-large'>
        <h2 className='title'>login</h2>
        <div className='columns'>
          <div className='column is-medium'>
            <div className='notification'>
              <p>Login to your account here.</p><p>If you don't have an account, <Link to='/register'>register</Link>. If you forgot your password, <Link to='/reset'>get a new one</Link>. If you have created an account, but need to have the verification link resent, just login, and we'll ask you for it.</p>
            </div>
          </div>
          <div className='column'>
            <FormLogin onSubmit={this.onSubmit} />
          </div>
        </div>
      </div>
    )
  }
}

Login.route = {
  path: 'login',
  component: Login
}
