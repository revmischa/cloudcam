import React from 'react'
import { Link, browserHistory } from 'react-router'
import { SubmissionError } from 'redux-form'

import { register } from '../api/user'
import { error, success } from '../api/notification'
import FormUser from '../components/FormUser'

export default class Register extends React.Component {
  constructor (props) {
    super(props)
    this.onSubmit = this.onSubmit.bind(this)
  }

  onSubmit (form) {
    return register({
      username: form.username,
      password: form.password,
      email: form.email
    })
    .then(u => {
      success('New user created. Please check your email.')
      browserHistory.push('/login')
    })
    .catch(e => {
      let msg = e.message || 'An error occurred.'
      let field = false
      if (msg.search('Password') !== -1) {
        field = true
        throw new SubmissionError({password: msg})
      }
      if (msg.search('User') !== -1) {
        field = true
        throw new SubmissionError({username: msg})
      }
      if (!field) {
        error(msg)
      }
    })
  }

  render () {
    return (
      <div className='section is-large form'>
        <h2 className='title'>join us</h2>
        <div className='columns'>
          <div className='column is-medium'>
            <div className='notification'>
              <p>Create an account here.</p><p>If you already have one, <Link to='/login'>login</Link>. If you forgot your password, <Link to='/reset'>reset it</Link>. If you have created an account, but need to have the verification link resent, just <Link to='/login'>login</Link>.</p>
            </div>
          </div>
          <div className='column'>
            <FormUser type='register' onSubmit={this.onSubmit} />
          </div>
        </div>
      </div>
    )
  }
}

Register.route = {
  path: 'register',
  component: Register
}
