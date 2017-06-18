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

  if (!values.password) {
    errors.password = 'Required.'
  }

  return errors
}

const FormLogin = (props) => {
  const { handleSubmit, invalid, pristine, submitting } = props
  return (
    <form onSubmit={handleSubmit}>
      <Field name='username' type='text' component={BasicField} label='username' />
      <Field name='password' type='password' component={BasicField} label='password' />
      <div className='control'>
        <button className={'button is-primary is-large' + (submitting ? ' is-loading' : '')} type='submit' disabled={invalid || pristine || submitting}>login</button>
      </div>
    </form>
  )
}

export default reduxForm({
  form: 'FormLogin',
  validate
})(FormLogin)
