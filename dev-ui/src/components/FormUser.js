import React from 'react'
import { Field, reduxForm } from 'redux-form'
import BasicField from './BasicField'

const validate = values => {
  const errors = {}
  if (!values.username) {
    errors.username = 'Required'
  } else if (values.username.length > 15) {
    errors.username = 'Must be 15 characters or less'
  }
  if (!values.email) {
    errors.email = 'Required.'
  } else if (!/^[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,42}$/i.test(values.email)) {
    errors.email = 'Invalid email address.'
  }

  if (!values.password) {
    errors.password = 'Required.'
  }

  if (!values.password2) {
    errors.password2 = 'Required.'
  }

  if (values.password !== values.password2) {
    errors.password2 = 'Must match password.'
  }

  return errors
}

const FormUser = (props) => {
  const { handleSubmit, invalid, pristine, submitting, type } = props
  console.log(props)
  return (
    <form onSubmit={handleSubmit}>
      <Field name='username' type='text' component={BasicField} label='username' />
      <Field name='email' type='email' component={BasicField} label='email' />
      <Field name='password' type='password' component={BasicField} label='password' />
      <Field name='password2' type='password' component={BasicField} label='password (again)' />
      <div className='control'>
        <button className={'button is-primary is-large' + (submitting ? ' is-loading' : '')} type='submit' disabled={invalid || pristine || submitting}>{type || 'save'}</button>
      </div>
    </form>
  )
}

export default reduxForm({
  form: 'FormUser',
  validate
})(FormUser)
